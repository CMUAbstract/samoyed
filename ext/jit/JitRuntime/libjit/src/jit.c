#include <stdarg.h>
#include <string.h>
#include <msp430.h>
#include <libcapybara/power.h>
#include <libmsp/periph.h>
#include <libio/console.h>
#include <stdlib.h>

#include "jit.h"

__nv uint16_t energy_overflow = 0;
__nv uint16_t energy_counter = 0;
//__nv uint8_t chkpt_ever_taken = 0;
__nv volatile uint8_t chkpt_mask = 0;
__nv volatile uint8_t chkpt_mask_init = 0;
__nv unsigned undoLogBound = 65535;

__nv uint8_t undoLogBuffer[ULOG_BUF_SIZE];
__nv uint8_t* undoLogSrcAddr[ULOG_BUF_SIZE/2]; // assume each ARR backup is at least size 2
__nv unsigned undoLogSize[ULOG_BUF_SIZE/2]; // assume each ARR backup is at least size 2

__nv uint8_t* undoLogPtr = undoLogBuffer;
__nv uint8_t* undoLogPtr_tmp = undoLogBuffer;
__nv uint8_t* undoLogPtr_bak; // for undo-logging undoLogPtr

__nv unsigned undoLogCnt = 0;
__nv unsigned undoLogCnt_tmp = 0;
__nv unsigned undoLogCnt_bak; // for undo-logging undoLogCnt:w

// TODO: cur limitation. Knob can only be 2 byte unsigned
// There is no point of knob being signed.
// however, it might want to be 4 bytes.
// expanding is tedious since we only need one more pointer
// and the frontend can choose to use between the two.
// I opted not to implement it because in reality it barely overflows.
__nv unsigned* cur_K_addr = NULL;
// default search function
void binary_search(unsigned* K) {
	(*K) >>= 1;
}
// user defined search function (if any)
__nv void (*search_func)(unsigned*) = NULL;
__nv uint8_t chkpt_taken = 0;
__nv unsigned livelock_cnt = 0;
__nv int _jit_no_progress = 0;
__nv int _jit_bndMayNeedUpdate = 0;
__nv int _jit_no_progress_ulog = 0;
__nv int _jit_bndMayNeedUpdate_ulog = 0;
#define LIVELOCK_RETRY 3

void _setSearchFunc(unsigned (*custom_func)(unsigned*)) {
	if (!custom_func)
		search_func = &binary_search;
	else
		search_func = custom_func;
}

#define RAND_LCG_A 1103515245ULL
#define RAND_LCG_C 12345

__nv static volatile unsigned _jit_seed = 42;

unsigned my_rand()
{
  _jit_seed = ((unsigned)RAND_LCG_A) * _jit_seed + RAND_LCG_C;
  return _jit_seed >> 8; // do not use low-order bits
}

// Function for energy measurement
void fill_with_rand(char* src, size_t size) {
	for (unsigned i = 0; i < size; ++i) {
		unsigned rand = my_rand();
		rand &= 0xFF;
		char crand = (char)rand;
		*(src + i) = rand;
	}
}


void restoreWAR() {
	while (undoLogCnt) {
		uint8_t* src = undoLogSrcAddr[undoLogCnt - 1];
		unsigned size = undoLogSize[undoLogCnt - 1];
		uint8_t* dst = undoLogPtr - size;

		// copy using DMA!
		__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "m"(dst));
		__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "m"(src));
		// size in word
		DMA1SZ = size >> 1;
		DMA1CTL = (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);
		undoLogCnt--;
		undoLogPtr -= size;
	}
}

void restore_regs() {
	unsigned R0;
	//P1OUT |= BIT2;
	__asm__ volatile ("MOV &0x4400, %0":"=m"(R0));
	if (R0 == 0xFFFF) {
		return;
	}
	// this should be unset after restoring..
	chkpt_mask_init = 0;

	// WAR restoring routine
	// TODO: Currently we assume we can do this at once
	// if not, it also has to be recursive...
	restoreWAR();

#if 1
	if (!chkpt_taken) {
		// No checkpoint was taken
		// Possible livelock over protected region
		if (livelock_cnt >= LIVELOCK_RETRY) {
			// Assume we are in livelock
			_jit_no_progress = 1;
			_jit_bndMayNeedUpdate = 1;
			_jit_no_progress_ulog = 1;
			_jit_bndMayNeedUpdate_ulog = 1;
		} else {
			// if this is the first time, simply retry
			livelock_cnt++;
		}
	} else {
		livelock_cnt = 0;
	}
	// reset chkpt check flag
	chkpt_taken = 0;
#endif


	// First, restore R1
	// and increment it by 2		
	__asm__ volatile ("MOVX.A &0x4404, R1");
	//	__asm__ volatile ("ADD #52, R1");
	//__asm__ volatile ("MOV &0x4408, R2");
	__asm__ volatile ("MOVX.A &0x4410, R4");
	__asm__ volatile ("MOVX.A &0x4414, R5");
	__asm__ volatile ("MOVX.A &0x4418, R6");
	__asm__ volatile ("MOVX.A &0x441c, R7");
	__asm__ volatile ("MOVX.A &0x4420, R8");
	__asm__ volatile ("MOVX.A &0x4424, R9");
	__asm__ volatile ("MOVX.A &0x4428, R10");
	__asm__ volatile ("MOVX.A &0x442c, R11");
	__asm__ volatile ("MOVX.A &0x4430, R12");
	__asm__ volatile ("MOVX.A &0x4434, R13");
	__asm__ volatile ("MOVX.A &0x4438, R14");
	__asm__ volatile ("MOVX.A &0x443c, R15");
	//P1OUT &= ~BIT2;
	__asm__ volatile ("MOV &0x4408, R2");
	__asm__ volatile ("MOV &0x4400, R0");
}

void checkpoint() {
	//P1OUT |= BIT2;
	// We need to mask capy interrupt before doing
	// anything. This is because this checkpoint SHOULD
	// always succeed. There is no protection if this fails
	// (even with double buffering, because the control
	// flow should never go back)
	// worst case, the checkpoint will finish (hopefully, because
	// if interrupt fired in here, we know we had more than threshold
	// voltage when entering the checkpoint
	// but we will enter cold start.
	chkpt_taken = 1;
	_jit_no_progress = 0;
	_jit_no_progress_ulog = 0;
	__asm__ volatile ("MOV 0(R1), &0x4400");//r0
	__asm__ volatile ("MOV R2, &0x4408");//r2
	// if restore happens here, it means
	// possibly bit 5 of SR is set (CPUOFF)
	// which means checkpoint happened while sleeping
	__asm__ volatile ("MOVX.A R4, &0x4410");//r4
	__asm__ volatile ("MOVX.A R5, &0x4414");//r5
	__asm__ volatile ("MOVX.A R6, &0x4418");
	__asm__ volatile ("MOVX.A R7, &0x441c");
	__asm__ volatile ("MOVX.A R8, &0x4420");
	__asm__ volatile ("MOVX.A R9, &0x4424");
	__asm__ volatile ("MOVX.A R10, &0x4428");
	__asm__ volatile ("MOVX.A R11, &0x442c");
	__asm__ volatile ("MOVX.A R12, &0x4430");
	__asm__ volatile ("MOVX.A R13, &0x4434");
	__asm__ volatile ("MOVX.A R14, &0x4438");
	__asm__ volatile ("MOVX.A R15, &0x443c");
	__asm__ volatile ("ADD #4, R1");
	__asm__ volatile ("MOVX.A R1, &0x4404"); //r1
	__asm__ volatile ("SUB #4, R1");
	// This makes system correct, but not too efficient
	// because the charge time is slow even if you shutdown with
	// cap partially filled
	//capybara_shutdown();
	//P1OUT &= ~BIT2;
}

int undoLog(uint8_t* src, size_t size) {
	undoLogPtr_tmp = undoLogPtr_bak; // for undo-logging undoLogPtr
	undoLogCnt_tmp = undoLogCnt_bak;
	if (size <= undoLogBound && !_jit_no_progress_ulog) {
		// copy using DMA!
		__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "m"(src));
		__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "m"(undoLogPtr));
		// size in word
		DMA1SZ = size >> 1;
		DMA1CTL = (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);
		undoLogPtr_tmp += size;

		undoLogSrcAddr[undoLogCnt_tmp] = src;
		undoLogSize[undoLogCnt_tmp] = size;

		undoLogCnt_tmp++;
		if (_jit_bndMayNeedUpdate_ulog) {
			undoLogBound = size;
			_jit_bndMayNeedUpdate_ulog = 0;
		}
		return 1;
	}
	return 0;
}

void recursiveUndoLog(uint8_t* src, size_t size) {
	int success = 0;
	undoLogPtr_bak = undoLogPtr_tmp;
	undoLogCnt_bak = undoLogCnt_tmp;
	PROTECT_BEGIN();
	success = undoLog(src, size);
	PROTECT_END_NOWARCLR();
	if (!success) {
		undoLog(src, size >> 1);
		undoLog(src + (size >> 1), size >> 1);
	}
}

#ifndef JIT_H
#define JIT_H

#include <stddef.h>
#include <stdint.h>
#include <libmsp/mem.h>

#define ULOG_BUF_SIZE 5000 // size in Byte
extern unsigned* cur_K_addr;
extern void (*search_func)(unsigned*);
extern uint8_t chkpt_taken;
extern int _jit_no_progress;
extern int _jit_bndMayNeedUpdate;
extern uint8_t* undoLogPtr;
extern uint8_t* undoLogPtr_bak; // for undo-logging undoLogPtr
extern unsigned undoLogCnt;
extern unsigned undoLogCnt_tmp;
extern uint8_t* undoLogPtr_tmp;
extern unsigned undoLogCnt_bak; // for undo-logging undoLogCnt
extern uint8_t undoLogBuffer[ULOG_BUF_SIZE];

void fill_with_rand(char* src, size_t size);
void _setSearchFunc(unsigned custom_func(unsigned*));
void restore_regs();
int undoLog(uint8_t* src, size_t size); // size in byte
//extern uint8_t chkpt_ever_taken;
// This has to be volatile to make it work correctly
extern volatile uint8_t chkpt_mask;
extern volatile uint8_t chkpt_mask_init;
extern uint16_t energy_overflow;
extern uint16_t energy_counter;

// We need to mask capy interrupt before doing
// mandatory checkpoint. This is because this checkpoint SHOULD
// always succeed. There is no protection if this fails
// (even with double buffering, because the control
// flow should never go back)
// Worst case, the checkpoint will finish (hopefully, because
// if interrupt fired in here, we know we had more than threshold
// voltage when entering the checkpoint -- this can be safely
// assumed by tuning the threshold)
// but we will enter cold start.

// Not sure why, but disabling and enabling interrupt of the
// comparator sometimes make comparator interrupt to miss
// Don't know why, but disabling and enabling GIE fixes the problem
//#define PROTECT_BEGIN() \
//	COMP_VBANK(INT) &= ~COMP_VBANK(IE);\
//	chkpt_mask = 1;\
//	checkpoint();\
//	COMP_VBANK(INT) |= COMP_VBANK(IE);
#define PROTECT_BEGIN() \
	__disable_interrupt();\
	chkpt_mask = 1;\
	checkpoint();\
	__enable_interrupt();\


#define PROTECT_END() \
	chkpt_mask = 0;\
	undoLogCnt = 0;\
	undoLogCnt_tmp = 0;\
	undoLogPtr = undoLogBuffer;\
	undoLogPtr_tmp = undoLogBuffer;\

#define PROTECT_END_NOWARCLR() \
	chkpt_mask = 0;\

//#define PARAM(...)
//#define IN(...)
//#define OUT(...)

#endif // JIT_H

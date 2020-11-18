#include <msp430.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef LOGIC
#define LOG(...)
#define PRINTF(...)
#define BLOCK_PRINTF(...)
#define BLOCK_PRINTF_BEGIN(...)
#define BLOCK_PRINTF_END(...)
#define INIT_CONSOLE(...)
#else
#include <libio/console.h>
#endif
#include <libmsp/mem.h>
#include <libmsp/periph.h>
#include <libmsp/clock.h>
#include <libmsp/watchdog.h>
#include <libmsp/gpio.h>
#include <libcapybara/capybara.h>
#include <libcapybara/power.h>
#include <libjit/jit.h>
#include <libmspdriver/driverlib.h>

#include "pins.h"
#define CONT_POWER 0
#define QUICKRECALL 0
//#define ARR_SIZE 50000
// Clang complains about too large array.
// For recursive, over 6250 has a problem so let's not make it too large
#define ARR_SIZE 50000
#define LOG(...)

void init();
volatile __attribute__((section(".upper.nv_vars"))) int data_src[ARR_SIZE] = {
#include "./src.txt"
};
volatile __attribute__((section(".upper.nv_vars"))) int data_dst[ARR_SIZE];
void dma_transfer(int* dst, int* src, unsigned size) {
	/* For some reason, below code 

		 DMA1SA = src;
		 DMA1DA = dst;

		 does not work. In GCC -O1, it is translated to
		 something like (assuming DMA1SA is 0x0522)
		 MOVX.W (lower part of R13), 0x0522
		 MOVX.W (upper part of R13), 0x0524
		 However, the second line does not work (do nothing)
		 Is it because 0x0522 is MMIO and shouldn't be accessed like
		 0x0524? ANYWAY, doing it manually */

	// r13 holds src, r12 holds dest
	__asm__ volatile ("MOVX.A R13, %0":"=m"(DMA1SA));
	__asm__ volatile ("MOVX.A R12, %0":"=m"(DMA1DA));
	// size in word
	DMA1SZ = size;
	uint16_t interruptState = __get_interrupt_state();
	__disable_interrupt();
	DMA1CTL |= (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ | DMAIE);
	__bis_SR_register(GIE + LPM0_bits); 
	__set_interrupt_state(interruptState);
}

__nv unsigned int K = ARR_SIZE;
int main()
{
	// Assume this region never gets an power interrupt.
	// Or is this really needed? (TODO)
	chkpt_mask_init = 1; // TODO: for now, special case for init ( But do we need this at all? )
	init();
	restore_regs();
	chkpt_mask_init = 0;

	PROTECT_BEGIN();
	capybara_config_banks(0x0);
	PROTECT_END();

	while (1) {
		// memset cannot handle too large data (size overflow)
		//memset(&data_dst[0], 0, ARR_SIZE*sizeof(data_dst[0]));
		// for simple test only,
		data_dst[ARR_SIZE - 1] = 7;

#ifdef LOGIC
	GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_0);
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_0);
#else
		PROTECT_BEGIN();
		PRINTF("start %u\r\n", data_dst[ARR_SIZE - 1]);
		PROTECT_END();
#endif

#if QUICKRECALL == 1
		dma_transfer(&data_dst[0], &data_src[0], ARR_SIZE);	
#else

		int* dst_new = &data_dst[0];
		int* src_new = &data_src[0];
		unsigned W = 0;
		cur_K_addr = &K;

		while (W < ARR_SIZE) {
			if (W + K <= ARR_SIZE) {
				PROTECT_BEGIN();
				dma_transfer(dst_new, src_new, K);
				PROTECT_END();

				dst_new = dst_new + K;
				src_new = src_new + K;
				W = W + K;
			} else {
				PROTECT_BEGIN();
				dma_transfer(dst_new, src_new, ARR_SIZE - W);
				PROTECT_END();

				W = ARR_SIZE;
			}
		}

		cur_K_addr = NULL;
		//PROTECT_BEGIN();
		//dma_transfer(&data_dst[0], &data_src[0], KNOB0(ARR_SIZE));	
		//PROTECT_IF_FAIL();
		//dma_transfer(ARG(0), ARG(1), KNOB0() >> 1);
		//dma_transfer(ARG(0) + (KNOB0() >> 1), ARG(1) + (KNOB0() >> 1), KNOB0() >> 1);
		//PROTECT_END();
#endif

#ifdef LOGIC
		GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_2);
		GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_2);
#else
		PROTECT_BEGIN();
		PRINTF("end %u\r\n", data_dst[ARR_SIZE-1]);
		PROTECT_END();
#endif
	}
	return 0;

}

void __attribute__((interrupt(DMA_VECTOR)))dmaIsrHandler(void) {
	if (DMA1CTL & DMAIFG)
		DMA1CTL &= ~DMAIFG;
	__bic_SR_register_on_exit(LPM0_bits);
}

void init()
{
	msp_watchdog_disable();
	msp_gpio_unlock();
	__enable_interrupt();
	capybara_wait_for_supply();
	capybara_config_pins();
	msp_clock_setup();

#if CONT_POWER == 0
	cb_rc_t deep_discharge_status = capybara_shutdown_on_deep_discharge();
	if (deep_discharge_status == CB_ERROR_ALREADY_DEEPLY_DISCHARGED)
		capybara_shutdown();
#endif

	INIT_CONSOLE();
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_DBG0);
	GPIO(PORT_DBG, DIR) |= BIT(PIN_DBG0);
#ifdef LOGIC
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_0);
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_1);
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_2);
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_DBG0);

	GPIO(PORT_DBG, DIR) |= BIT(PIN_AUX_0);
	GPIO(PORT_DBG, DIR) |= BIT(PIN_AUX_1);
	GPIO(PORT_DBG, DIR) |= BIT(PIN_AUX_2);
	GPIO(PORT_DBG, DIR) |= BIT(PIN_DBG0);

	GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_1);
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_1);
#else
	unsigned pc;
	__asm__ volatile ("MOV &0x4400, %0":"=m"(pc));
	PRINTF(".%x.%u\r\n", pc, K);
#endif
}

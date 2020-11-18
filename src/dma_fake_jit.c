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
#define ARR_SIZE 10000

#define LOG(...)

void init();

volatile __attribute__((section(".upper.nv_vars"))) int data_src[ARR_SIZE] = {
#include "./src.txt"
};
volatile __attribute__((section(".upper.nv_vars"))) int data_dst[ARR_SIZE];

void dma_transfer(int* dst, int* src, unsigned size) {
	for (unsigned i = 0; i < size; ++i)
		*(dst+i) = *(src+i);
}

__nv unsigned int _jitK0 = ARR_SIZE;
int _jit_in0dma_transfer(int * _arg0, int * _arg1, unsigned int _arg2)  {
	if (_arg2 <= _jitK0 && !_jit_no_progress) {
		dma_transfer(_arg0, _arg1, _arg2);
		_jitK0 = _arg2;
		return 1;
	}
	return 0;
}
void _jit_out0dma_transfer(int * _arg0, int * _arg1, unsigned int _arg2)  {
	int success = 0;
	PROTECT_BEGIN();
	success = _jit_in0dma_transfer(_arg0, _arg1, _arg2);
	PROTECT_END();
	if (!success) {
		;
		_jit_out0dma_transfer(_arg0, _arg1, _arg2 >> 1);	
		_jit_out0dma_transfer(_arg0 + (_arg2 >> 1), _arg1 + (_arg2 >> 1), _arg2 >> 1);	
		;
	}
}
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
		// memset(&data_dst[0], 0, ARR_SIZE*sizeof(data_dst[0]));
		// for simple test only,
		data_dst[ARR_SIZE - 1] = 0;

		PROTECT_BEGIN();
		PRINTF("start %u\r\n", data_dst[ARR_SIZE - 1]);
		PROTECT_END();

#if QUICKRECALL == 1
		dma_transfer(&data_dst[0], &data_src[0], ARR_SIZE);	
#else
		_jit_out0dma_transfer(&data_dst[0], &data_src[0], ARR_SIZE);
;
		//	SAFE_BEGIN();
		//		dma_transfer(&data_dst[0], &data_src[0], KNOB(ARR_SIZE));	
		//		SAFE_IF_FAIL();
		//		dma_transfer(ARG(0), ARG(1), KNOB() >> 2);	
		//		dma_transfer(ARG(0) + (KNOB() >> 2), ARG(1) + (KNOB() >> 2), KNOB() >> 2);	
		//		SAFE_END();
		//		SAFE_BEGIN();
		//		dma_transfer(&data_dst[0], &data_src[0], KNOB(ARR_SIZE));	
		//		UPDATE(0, ARG() + KNOB());
		//		UPDATE(1, ARG() + KNOB());
		//		SAFE_END();
		// should be translated into
		/*
			 int* dst_new = &data_dst[0];
			 int* src_new = &data_src[0];
			 unsigned W = 0;
			 cur_K_addr = &K;

			 while (W < ARR_SIZE) {
			 if (W + K <= ARR_SIZE) {
			 SAFE_START();
			 dma_transfer(dst_new, src_new, K);
			 SAFE_END();

			 dst_new = dst_new + K;
			 src_new = src_new + K;
			 W = W + K;
			 } else {
			 SAFE_START();
			 dma_transfer(dst_new, src_new, ARR_SIZE - W);
			 SAFE_END();

			 W = ARR_SIZE;
			 }
			 }

			 cur_K_addr = NULL;*/
		// translated code end
#endif
		PROTECT_BEGIN();
		PRINTF("end %u\r\n", data_dst[ARR_SIZE-1]);
		PROTECT_END();
	}

	return 0;

}

void init()
{
	msp_watchdog_disable();
	msp_gpio_unlock();
	__enable_interrupt();
	capybara_wait_for_supply();
	capybara_config_pins();
	msp_clock_setup();

	//	GPIO(PORT_DBG, DIR) |= BIT(PIN_AUX_0);
	//	GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_0);

	// Setup deep discharge shutdown interrupt before reconfiguring banks,
	// so that if we deep discharge during reconfiguration, we still at
	// least try to avoid cold start. NOTE: this is controversial./
#if CONT_POWER == 0
	cb_rc_t deep_discharge_status = capybara_shutdown_on_deep_discharge();

	// We still want to attempt reconfiguration, no matter how low Vcap
	// is, because otherwise we'd never be able to reconfigure. But, we
	// definitely do not want to run Vcap into the ground if we know
	// it is already below deep discharge threshold. NOTE: this doesn't
	// seem to matter much, because Vcap eventually discharges anyway
	// (regardless of whether we shutdown or not), if deep discharge threshold
	// was crossed.
	if (deep_discharge_status == CB_ERROR_ALREADY_DEEPLY_DISCHARGED)
		capybara_shutdown();
#endif

	INIT_CONSOLE();
	// Maliciously drain current through P1.3
	// attach a resistor to P1.3
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_DBG0);
	GPIO(PORT_DBG, DIR) |= BIT(PIN_DBG0);
	//	GPIO(PORT_DBG, OUT) |= BIT(PIN_DBG0);
	//	// and also P1.0
	//	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_0);
	//	GPIO(PORT_DBG, DIR) |= BIT(PIN_AUX_0);
#ifdef LOGIC
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_0);
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_1);
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_2);
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_DBG0);

	GPIO(PORT_DBG, DIR) |= BIT(PIN_AUX_0);
	GPIO(PORT_DBG, DIR) |= BIT(PIN_AUX_1);
	GPIO(PORT_DBG, DIR) |= BIT(PIN_AUX_2);
	GPIO(PORT_DBG, DIR) |= BIT(PIN_DBG0);

	//	GPIO(PORT_DBG, SEL0) &= 0;
	//	GPIO(PORT_DBG, SEL1) &= 0;

	GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_1);
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_1);
#else
	unsigned pc;
	__asm__ volatile ("MOV &0x4400, %0":"=m"(pc));
	PRINTF(".%x.%u\r\n", pc, _jitK0);
#endif
}

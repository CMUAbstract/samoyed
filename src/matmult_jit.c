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
#include <libdsp/DSPLib.h>

#ifdef CONFIG_EDB
#include <libedb/edb.h>
#else
#define ENERGY_GUARD_BEGIN()
#define ENERGY_GUARD_END()
#endif

#include "pins.h"
#define CONT_POWER 0
#define QUICKRECALL 0

#define S1_RS 16
#define S1_CS 16
#define S2_RS 16
#define S2_CS 16
#define D_RS S1_RS
#define D_CS S2_CS

#define LOG(...)
//#define LOG(...) \
//	PROTECT_BEGIN();\
//	printf(__VA_ARGS__);\
//	PROTECT_END();

__nv unsigned debug_cntr = 0;
__nv unsigned int _jitK1_0 = S2_CS;
__nv unsigned int _jitK0_0 = S1_RS;
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
	PRINTF(".%x.%u %u\r\n", pc, _jitK0_0, _jitK1_0);
#endif
}

#define CONFIG_SAMPLE_SIZE 600

//__nv int dst_nv[ROWSIZE][COLSIZE];
//__nv int src_nv[ROWSIZE][COLSIZE] = {{1, 2}, {3, 4}};
__nv int src1[S1_RS][S1_CS] = {
#include "./src.txt"
};
__nv int src2[S2_RS][S2_CS] = {
#include "./src.txt"
};
__nv int dest[D_RS][D_CS] = {{1,2}, {3,4}};
__nv int dest2[D_RS][D_CS] = {{1,2}, {3,4}};
// fake app for testing
void matmult(const int* src1, const int* src2, int* dst,
		const unsigned src1_row_size, const unsigned src1_col_size,
		const unsigned src2_row_size, const unsigned src2_col_size) {
	for (unsigned r = 0; r < src1_row_size; ++r) {
		for (unsigned c = 0; c < src2_col_size; ++c) {
			*(dst + r*D_CS + c) = 0;
			for (unsigned rr = 0; rr < src1_col_size; ++rr) {
				*(dst + r*S2_CS + c) +=
					(*(src1 + r*S1_CS + rr)) * (*(src2 + rr*S2_CS + c));
			}
		}
	}
}
void matmult_large(const int* src1, const int* src2, int* dst,
		const unsigned src1_row_size, const unsigned src1_col_size,
		const unsigned src2_row_size, const unsigned src2_col_size) {
	if (src1_col_size != src2_row_size) {
		PRINTF("error %u %u\r\n", src1_col_size, src2_row_size);
		while(1); // error!
	}
	matmult(src1, src2, dst, src1_row_size, src1_col_size, src2_row_size, src2_col_size);
}
int _jit_in0matmult_large(const int * _arg0, const int * _arg1, int * _arg2, unsigned int _arg3, const unsigned int _arg4, const unsigned int _arg5, unsigned int _arg6)  {
	if (_arg3 <= _jitK0_0 && _arg6 <= _jitK1_0 && !_jit_no_progress) {
		matmult_large(_arg0, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6);
		_jitK0_0 = _arg3;
		_jitK1_0 = _arg6;
		return 1;
	}
	return 0;
}
void _jit_out0matmult_large(const int * _arg0, const int * _arg1, int * _arg2, unsigned int _arg3, const unsigned int _arg4, const unsigned int _arg5, unsigned int _arg6)  {
	int success = 0;
	PROTECT_BEGIN();
	success = _jit_in0matmult_large(_arg0, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6);
	PROTECT_END();
	if (!success) {
		;
		if (_arg3 > _arg6) {
			_jit_out0matmult_large(_arg0, _arg1, _arg2, _arg3 >> 1, _arg4, _arg5, _arg6);
			_jit_out0matmult_large(_arg0 + (_arg3 >> 1)*S1_CS, _arg1, _arg2 + (_arg3 >> 1)*D_CS, _arg3 >> 1, _arg4, _arg5, _arg6);
		}
		else {
			_jit_out0matmult_large(_arg0, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6 >> 1);
			_jit_out0matmult_large(_arg0, _arg1 + (_arg6 >> 1), _arg2 + (_arg6 >> 1), _arg3, _arg4, _arg5, _arg6 >> 1);
		}

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

	while (1) {
		dest[D_RS-1][D_CS-1] = 7;
		PROTECT_BEGIN();
		PRINTF("start %u\r\n", dest[D_RS-1][D_CS-1]);
		PROTECT_END();

#if QUICKRECALL == 1
#else
		// this is HW. But now it is all software..for testing man..
		_jit_out0matmult_large(src1, src2, dest, S1_RS, S1_CS, S2_RS, S2_CS);
;
		// software ref -- slow but always correct
		matmult(*src1, *src2, *dest2, S1_RS, S1_CS, S2_RS, S2_CS);
		/*
		SAFE_BEGIN();
		matmult(src1, src2, dest2, S1_RS, S1_CS, S2_RS, S2_CS);
		SAFE_IF_FAIL();
		if (KNOB0() > KNOB1()) {
			matmult(ARG(0), ARG(1), ARG(2), KNOB0() >> 1, ARG(4), ARG(5), ARG(6));
			matmult(ARG(0) + (KNOB0() >> 1)*S1_CS, ARG(1), ARG(2) + (KNOB0() >> 1)*D_CS, KNOB0() >> 1, ARG(4), ARG(5), ARG(6));
		}
		else {
			matmult(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), KNOB1() >> 1);
			matmult(ARG(0), ARG(1) + (KNOB1() >> 1), ARG(2) + (KNOB1() >> 1), ARG(3), ARG(4), ARG(5), KNOB1() >> 1);
		}
		SAFE_END();
		*/
#endif
		for (unsigned i = 0; i < D_RS; ++i) {
			for (unsigned j = 0; j < D_CS; ++j) {
				//PRINTF("%u ", dest[i][j]);
				if (dest[i][j] != dest2[i][j]) {
					PROTECT_BEGIN();
					PRINTF("ERROR!\r\n");
					PROTECT_END();
				}
			}
			//PRINTF("\r\n");
		}
//		for (unsigned i = 0; i < D_RS; ++i) {
//			for (unsigned j = 0; j < D_CS; ++j) {
//				PROTECT_BEGIN();
//				PRINTF("%u-%u ", dest[i][j], dest2[i][j]);
//				PROTECT_END();
//			}
//			PROTECT_BEGIN();
//			PRINTF("\r\n");
//			PROTECT_END();
//		}

		PROTECT_BEGIN();
		PRINTF("end\r\n");
		//PRINTF("end\r\n");
		PROTECT_END();
	}
	return 0;
}

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
#define SW 0
#define MM_ITER 60


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

__nv msp_matrix_mpy_q15_params mpyParams = {
	.srcARows = S1_RS,
	.srcACols = S1_CS,
	.srcBRows = S2_RS,
	.srcBCols = S2_CS
};




msp_status sw_matrix_mpy_q15(const msp_matrix_mpy_q15_params *params, const _q15 *srcA, const _q15 *srcB, _q15 *dst)
{
	uint16_t cntr;
	uint16_t srcARows;
	uint16_t srcACols;
	uint16_t srcBRows;
	uint16_t srcBCols;
	uint16_t dst_row;
	uint16_t dst_col;
	uint16_t row_offset;
	uint16_t col_offset;
	uint16_t dst_row_offset;

	/* Initialize the row and column sizes. */
	srcARows = params->srcARows;
	srcACols = params->srcACols;
	srcBRows = params->srcBRows;
	srcBCols = params->srcBCols;

#ifndef MSP_DISABLE_DIAGNOSTICS
	/* Check that column of A equals rows of B */
	if (srcACols != srcBRows) {
		return MSP_SIZE_ERROR;
	}
#endif //MSP_DISABLE_DIAGNOSTICS

	/* In initialize loop counters. */
	cntr = 0;
	dst_row = 0;
	dst_col = 0;
	row_offset = 0;
	col_offset = 0;
	dst_row_offset = 0;

#if defined(__MSP430_HAS_MPY32__)
	/* If MPY32 is available save control context, set to fractional mode, set saturation mode. */
	uint16_t ui16MPYState = MPY32CTL0;
	MPY32CTL0 = MPYFRAC | MPYDLYWRTEN | MPYSAT;

	/* Loop through all srcA rows. */
	while(srcARows--) {
		/* Loop through all srcB columns. */
		while (dst_col < srcBCols) {
			/* Reset result accumulator. */
			MPY32CTL0 &= ~MPYC;
			RESLO = 0; RESHI = 0;

			/* Loop through all elements in srcA column and srcB row. */
			while(cntr < srcACols) {
				MACS = srcA[row_offset + cntr];
				OP2 = srcB[col_offset + dst_col];
				col_offset += srcBCols;
				cntr++;
			}

			/* Store the result */
			dst[dst_row_offset + dst_col] = RESHI;

			/* Update pointers. */
			dst_col++;
			cntr = 0;
			col_offset = 0;
		}

		/* Update pointers. */
		dst_row++;
		dst_col = 0;
		row_offset += srcACols;
		dst_row_offset += srcBCols;
	}

	/* Restore MPY32 control context, previous saturation state. */
	MPY32CTL0 = ui16MPYState;

#else //__MSP430_HAS_MPY32__
	_iq31 result;

	/* Loop through all srcA rows. */
	while(srcARows--) {
		/* Loop through all srcB columns. */
		while (dst_col < srcBCols) {
			/* Initialize accumulator. */
			result = 0;

			/* Loop through all elements in srcA column and srcB row. */
			while(cntr < srcACols) {
				result += (_iq31)srcA[row_offset + cntr] * (_iq31)srcB[col_offset + dst_col];
				col_offset += srcBCols;
				cntr++;
			}

			/* Saturate and store the result */
			dst[dst_row_offset + dst_col] = (_q15)__saturate(result >> 15, INT16_MIN, INT16_MAX);

			/* Update pointers. */
			dst_col++;
			cntr = 0;
			col_offset = 0;
		}

		/* Update pointers. */
		dst_row++;
		dst_col = 0;
		row_offset += srcACols;
		dst_row_offset += srcBCols;
	}
#endif //__MSP430_HAS_MPY32__

	return MSP_SUCCESS;
}

__nv unsigned debug_cntr = 0;
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
	PRINTF(".%x.%u\r\n", pc, debug_cntr);
#endif
}

#define CONFIG_SAMPLE_SIZE 600

__nv _q15 src_nv[S1_RS][S1_CS] = {{_Q15(0.1), _Q15(0.2)}, {_Q15(0.3), _Q15(0.4)}};

DSPLIB_DATA(src1, 4) _q15 lea_src1[S1_RS][S1_CS];
DSPLIB_DATA(src2, 4) _q15 lea_src2[S2_RS][S2_CS];
DSPLIB_DATA(dest, 4) _q15 lea_dest[D_RS][D_CS];
__nv _q15 expected[D_RS][D_CS] = {{_Q15(0.07), _Q15(0.1)}, {_Q15(0.15), _Q15(0.22)}};

__atomic__ safe_matmult(__inout__[S1_RS*S1_CS*2] int* src1, __input__[S2_RS*S2_CS*2] int* src2) {
	msp_matrix_mpy_q15_params* params = &mpyParams;

	// copy src1 to src1
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "m"(src1));
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "i"(lea_src1));
	// size in word
	DMA1SZ = S1_RS*S1_CS;
	DMA1CTL |= (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);

	// copy src2 to src2
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "m"(src2));
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "i"(lea_src2));
	// size in word
	DMA1SZ = S2_RS*S2_CS;
	DMA1CTL |= (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);

	msp_matrix_mpy_q15(params, lea_src1, lea_src2, lea_dest);

	// copy dst
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "i"(lea_dest));
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "m"(src1));
	// size in word
	DMA1SZ = D_RS*D_CS;
	DMA1CTL |= (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);
}
/*
	 void safe_matmult(msp_matrix_mpy_q15_params* params, int* src1, int* src2) {
	 IF_ATOMIC();

// copy src1 to src1
__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "m"(src1));
__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "i"(lea_src1));
// size in word
DMA1SZ = S1_RS*S1_CS;
DMA1CTL |= (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);

// copy src2 to src2
__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "m"(src2));
__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "i"(lea_src2));
// size in word
DMA1SZ = S2_RS*S2_CS;
DMA1CTL |= (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);

status = msp_matrix_mpy_q15(params, lea_src1, lea_src2, lea_dest);

// copy dst
__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "i"(lea_dest));
__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "m"(src1));
// size in word
DMA1SZ = D_RS*D_CS;
DMA1CTL |= (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);

WAR(src1, S1_RS*S1_CS*2);
END_IF();
}
 */

int main()
{
	while (1) {
#ifdef LOGIC
		GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_0);
		GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_0);
#else
		PROTECT_BEGIN();
		PRINTF("start\r\n");
		PROTECT_END();
#endif
		for (unsigned ii = 0; ii < MM_ITER; ++ii) {
			/* Invoke the msp_matrix_mpy_q15 API. */
#if QUICKRECALL == 1
#elif SW == 1
			// Disable LEA!
			sw_matrix_mpy_q15(&mpyParams, *src_nv, *src_nv, *src_nv);
#else
			safe_matmult(*src_nv, *src_nv);
#endif
		}

#ifdef LOGIC
		GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_2);
		GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_2);
#else
		for (unsigned i = 0; i < 2; ++i) {
			for (unsigned j = 0; j < 2; ++j) {
				//PRINTF("%02x ", dest[i][j]);
				if (src_nv[i][j] != expected[i][j]) {
					PROTECT_BEGIN();
					PRINTF("x ");
					PROTECT_END();
				}
				else {
					PROTECT_BEGIN();
					PRINTF("o ");
					PROTECT_END();
				}
			}
			PROTECT_BEGIN();
			PRINTF("\r\n");
			PROTECT_END();
		}
		for (unsigned i = 0; i < 2; ++i) {
			for (unsigned j = 0; j < 2; ++j) {
				PROTECT_BEGIN();
				PRINTF("%02x:%02x ", src_nv[i][j], expected[i][j]);
				PROTECT_END();
			}
			PROTECT_BEGIN();
			PRINTF("\r\n");
			PROTECT_END();
		}

		PROTECT_BEGIN();
		PRINTF("end\r\n");
		//PRINTF("end\r\n");
		PROTECT_END();
#endif
	}

	return 0;
}

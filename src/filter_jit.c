/* --COPYRIGHT--,BSD
 * Copyright (c) 2016, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
//******************************************************************************
// Real 16-bit FIR filter.
//
//! \example filter_ex1_fir_q15.c
//! This example demonstrates how to use the msp_fir_q15 API and circular buffer
//! feature to filter 16-bit input data. The input signal is composed of two
//! generated sinusoidal signals added together, one sinusoid with a frequency
//! that will pass though the filter and one with a frequency that will be
//! filtered out. The inputs are copied into a circular buffer with twice the
//! length of the filter and allows input history from previous filter
//! operations to be reused without allocating and copying additional samples to
//! the start of the input data. The generated input and result can be compared
//! to see the effect of the filter.
//!
// Brent Peterson, Jeremy Friesenhahn
// Texas Instruments Inc.
// April 2016
//******************************************************************************
#include "msp430.h"

#include <math.h>
#include <stdint.h>
#include <string.h>
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
#include <libapds/proximity.h>
#include <libdsp/DSPLib.h>

#include "pins.h"
#define CONT_POWER 0
#define SW 0
//#define SIGNAL_LENGTH       256
//#define SIGNAL_LENGTH       16384
#define SIGNAL_LENGTH       2048
#define FILT_ITER 5

/* Include header generated from DSPLib GUI here (optional). */
//#include "myCoeffs_ex1.h"
void init();

__nv _q15 filt[16] = {
    _Q15(+0.001962), _Q15(-0.001728), _Q15(-0.012558), _Q15(-0.023557),
    _Q15(-0.004239), _Q15(+0.072656), _Q15(+0.189060), _Q15(+0.278404),
    _Q15(+0.278404), _Q15(+0.189060), _Q15(+0.072656), _Q15(-0.004239), 
    _Q15(-0.023557), _Q15(-0.012558), _Q15(-0.001728), _Q15(+0.001962)
};

DSPLIB_DATA(FILTER_COEFFS_EX1,4)
_q15 FILTER_COEFFS_EX1[16];
//DSPLIB_DATA(filt,4)
//_q15 filt[16] = {
//    _Q15(+0.001962), _Q15(-0.001728), _Q15(-0.012558), _Q15(-0.023557),
//    _Q15(-0.004239), _Q15(+0.072656), _Q15(+0.189060), _Q15(+0.278404),
//    _Q15(+0.278404), _Q15(+0.189060), _Q15(+0.072656), _Q15(-0.004239), 
//    _Q15(-0.023557), _Q15(-0.012558), _Q15(-0.001728), _Q15(+0.001962)
//};

/* Filter parameters */
#define FIR_LENGTH      64
#define COEFF_LENTH     sizeof(filt)/sizeof(filt[0])

/*
 * Circular buffer of size 2*FIR_LENGTH, aligned to 4*FIR_LENGTH in order to
 * use the circular buffer feature.
 */
#if SW == 0
DSPLIB_DATA(circularBuffer,MSP_ALIGN_FIR_Q15(FIR_LENGTH))
_q15 circularBuffer[2*FIR_LENGTH];
#else
__nv _q15 circularBuffer[2*FIR_LENGTH];
#endif

/* Generated input signal */
__attribute__((section(".upper.nv_vars")))
_q15 input[SIGNAL_LENGTH] = {
#include "filter_src.txt"
};
__attribute__((section(".upper.nv_vars")))
_q15 result_nv[SIGNAL_LENGTH];

/* Filter result */
DSPLIB_DATA(result,4)
_q15 result[FIR_LENGTH];

msp_status sw_fir_q15(const msp_fir_q15_params *params, const _q15 *src, _q15 *dst)
{
    uint16_t i;
    uint16_t j;
    uint16_t tapLength;
    uint16_t outputLength;
    bool enableCircBuf;
    const _q15 *srcPtr;
    const _q15 *coeffPtr;

    /* Save parameters to local variables. */
    tapLength = params->tapLength;
    outputLength = params->length;
    enableCircBuf = params->enableCircularBuffer;

#ifndef MSP_DISABLE_DIAGNOSTICS
    /* Check that tap and output length are a multiple of two. */
    if ((tapLength & 1) || (outputLength & 1)) {
        return MSP_SIZE_ERROR;
    }

    /* Check that the length is a power of two if circular buffer is enabled. */
    if (enableCircBuf && (outputLength & (outputLength-1))) {
        return MSP_SIZE_ERROR;
    }
#endif //MSP_DISABLE_DIAGNOSTICS

#if defined(__MSP430_HAS_MPY32__)
    /* If MPY32 is available save control context and set to fractional mode. */
    uint16_t ui16MPYState = MPY32CTL0;
    MPY32CTL0 = MPYFRAC | MPYDLYWRTEN | MPYSAT;
#endif //__MSP430_HAS_MPY32__

    /* Calculate filtered output using circular buffer. */
    if (enableCircBuf) {
        uintptr_t mask;
        const _q15 *srcStartPtr;
        const _q15 *srcEndPtr;

        /* Initialize circular buffer mask and set start pointer. */
        mask = (uintptr_t)(2*outputLength*sizeof(_q15) - 1);
        srcStartPtr = (const _q15 *)__circular_mask(src, mask);
        srcEndPtr = srcStartPtr + 2*outputLength;

        /* Calculate filtered output. */
        for (i = 0; i < outputLength; i++) {
            /* Reset data pointers and loop counters. */
            uint16_t j2;
            coeffPtr = &params->coeffs[tapLength-1];
            srcPtr = (const _q15 *)__circular_increment((const void *)src, i*sizeof(_q15), mask);
            j = srcEndPtr - srcPtr;
            j = j > tapLength ? tapLength : j;
            j2 = tapLength - j;

#if defined(__MSP430_HAS_MPY32__)
            /* Reset multiplier context. */
            MPY32CTL0 &= ~MPYC;
            RESLO = 0; RESHI = 0;

            /* Multiply and accumulate inputs and coefficients. */
            while (j--) {
                MACS = *srcPtr++;
                OP2 =  *coeffPtr--;
            }

            /* Multiply and accumulate inputs and coefficients after circular buffer loop. */
            srcPtr = srcStartPtr;
            while (j2--) {
                MACS = *srcPtr++;
                OP2 =  *coeffPtr--;
            }

            /* Store accumulated result. */
            *dst++ = RESHI;
#else //__MSP430_HAS_MPY32__
            /* Reset accumulator. */
            _iq31 result = 0;

            /* Multiply and accumulate inputs and coefficients. */
            while (j--) {
                result += (_iq31)*srcPtr++ * (_iq31)*coeffPtr--;
            }

            /* Multiply and accumulate inputs and coefficients after circular buffer loop. */
            srcPtr = srcStartPtr;
            while (j2--) {
                result += (_iq31)*srcPtr++ * (_iq31)*coeffPtr--;
            }

            /* Saturate accumulator and store result. */
            *dst++ = __saturate(result >> 15, INT16_MIN, INT16_MAX);
#endif //__MSP430_HAS_MPY32__
        }
    }
    /* Calculate filtered output without circular buffer. */
    else {
        for (i = 0; i < outputLength; i++) {
            /* Reset data pointers and loop counters. */
            srcPtr = &src[i];
            coeffPtr = &params->coeffs[tapLength-1];
            j = tapLength;

#if defined(__MSP430_HAS_MPY32__)
            /* Reset multiplier context. */
            MPY32CTL0 &= ~MPYC;
            RESLO = 0; RESHI = 0;

            /* Multiply and accumulate inputs and coefficients. */
            while (j--) {
                MACS = *srcPtr++;
                OP2 =  *coeffPtr--;
            }

            /* Store accumulated result. */
            *dst++ = RESHI;
#else //__MSP430_HAS_MPY32__
            /* Reset accumulator. */
            _iq31 result = 0;

            /* Multiply and accumulate inputs and coefficients. */
            while (j--) {
                result += (_iq31)*srcPtr++ * (_iq31)*coeffPtr--;
            }

            /* Saturate accumulator and store result. */
            *dst++ = __saturate(result >> 15, INT16_MIN, INT16_MAX);
#endif //__MSP430_HAS_MPY32__
        }
    }

     return MSP_SUCCESS;
}

msp_status sw_copy_q15(const msp_copy_q15_params *params, const _q15 *src, _q15 *dst)
{
    uint16_t length;

    /* Initialize the vector length. */
    length = params->length;
    while(length--) {
        *dst++ = *src++;
    }

    return MSP_SUCCESS;
}

msp_status sw_fill_q15(const msp_fill_q15_params *params, _q15 *dst)
{
    uint16_t length;

    /* Initialize the vector length. */
    length = params->length;
    while(length--) {
        *dst++ = params->value;
    }

    return MSP_SUCCESS;
}




__nv size_t _jit_0_size = 65535;
int _jit_safe_0(_q15 * src, _q15 * dst, size_t size)  {
	size_t size_bak = size;
	if (size_bak <= _jit_0_size && !_jit_no_progress) {
;
	;
	;
	;

		uint16_t samples;
		uint16_t copyindex;
		uint16_t filterIndex;
		msp_fir_q15_params firParams;
		msp_fill_q15_params fillParams;

		// Zero initialize FIR input for use with circular buffer.
		fillParams.length = FIR_LENGTH*2;
		fillParams.value = 0;
		msp_fill_q15(&fillParams, circularBuffer);

		// copy filter
		__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "i"(filt));
		__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "i"(FILTER_COEFFS_EX1));
		// size in word
		DMA1SZ = 16;
		DMA1CTL = (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);

		// Initialize the FIR parameter structure.
		firParams.length = FIR_LENGTH;
		firParams.tapLength = COEFF_LENTH;
		//firParams.coeffs = filt;
		firParams.coeffs = FILTER_COEFFS_EX1;
		firParams.enableCircularBuffer = true;
		// Initialize counters.
		samples = 0;
		copyindex = 0;
		filterIndex = 2*FIR_LENGTH - COEFF_LENTH;

		// Run FIR filter with 128 sample circular buffer.
		while (samples < size) {
			// Copy FIR_LENGTH samples to filter input.
			uint8_t* tmp0 = &src[samples];
			uint8_t* tmp1 = &circularBuffer[copyindex];
			__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "m"(tmp0));
			__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "m"(tmp1));
			// size in word
			DMA1SZ = FIR_LENGTH;
			DMA1CTL = (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);

			// Invoke the msp_fir_q15 function.
			//msp_fir_q15(&firParams, &circularBuffer[filterIndex], &result[samples]);
			msp_fir_q15(&firParams, &circularBuffer[filterIndex], result);

			// Copy result
			_q15* dst_addr = &dst[samples];
			__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "i"(result));
			__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "m"(dst_addr));
			// size in word
			DMA1SZ = FIR_LENGTH;
			DMA1CTL = (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);

			// Update counters.
			copyindex ^= FIR_LENGTH;
			filterIndex ^= FIR_LENGTH;
			samples += FIR_LENGTH;

		}


		if (_jit_bndMayNeedUpdate) {
			_jit_0_size = size_bak;
			_jit_bndMayNeedUpdate = 0;
		}
		return 1;
	}
	return 0;
}
void safe_filt(_q15* src, _q15* dst, size_t size) {
		int success = 0;
	if (src == dst) {
		if (size <= _jit_0_size) {
			recursiveUndoLog(src,  FIR_LENGTH << 1);
		}
	}
	undoLogPtr = undoLogPtr_tmp;
	undoLogCnt = undoLogCnt_tmp;
	PROTECT_BEGIN();
	undoLogPtr = undoLogPtr_tmp;
	undoLogCnt = undoLogCnt_tmp;
	success = _jit_safe_0(src, dst, size);
	PROTECT_END();
	if (!success) {
;

	safe_filt(src, dst, size >> 1);
	safe_filt(src + (size >> 1), dst + (size >> 1), size >> 1);

	}
;
}











/*
void safe_filt(_q15* src, _q15* dst, size_t size) {
	IF_ATOMIC(KNOB(size, SIGNAL_LENGTH));

		uint16_t samples;
		uint16_t copyindex;
		uint16_t filterIndex;
		msp_fir_q15_params firParams;
		msp_fill_q15_params fillParams;

		// Zero initialize FIR input for use with circular buffer.
		fillParams.length = FIR_LENGTH*2;
		fillParams.value = 0;
		msp_fill_q15(&fillParams, circularBuffer);

		// copy filter
		__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "i"(filt));
		__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "i"(FILTER_COEFFS_EX1));
		// size in word
		DMA1SZ = 16;
		DMA1CTL = (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);

		// Initialize the FIR parameter structure.
		firParams.length = FIR_LENGTH;
		firParams.tapLength = COEFF_LENTH;
		//firParams.coeffs = filt;
		firParams.coeffs = FILTER_COEFFS_EX1;
		firParams.enableCircularBuffer = true;
		// Initialize counters.
		samples = 0;
		copyindex = 0;
		filterIndex = 2*FIR_LENGTH - COEFF_LENTH;

		// Run FIR filter with 128 sample circular buffer.
		while (samples < size) {
			// Copy FIR_LENGTH samples to filter input.
			uint8_t* tmp0 = &src[samples];
			uint8_t* tmp1 = &circularBuffer[copyindex];
			__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "m"(tmp0));
			__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "m"(tmp1));
			// size in word
			DMA1SZ = FIR_LENGTH;
			DMA1CTL = (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);

			// Invoke the msp_fir_q15 function.
			//msp_fir_q15(&firParams, &circularBuffer[filterIndex], &result[samples]);
			msp_fir_q15(&firParams, &circularBuffer[filterIndex], result);

			// Copy result
			_q15* dst_addr = &dst[samples];
			__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "i"(result));
			__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "m"(dst_addr));
			// size in word
			DMA1SZ = FIR_LENGTH;
			DMA1CTL = (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);

			// Update counters.
			copyindex ^= FIR_LENGTH;
			filterIndex ^= FIR_LENGTH;
			samples += FIR_LENGTH;

		}


	ELSE();

	safe_filt(src, dst, size >> 1);
	safe_filt(src + (size >> 1), dst + (size >> 1), size >> 1);

	END_IF();
}
*/

void safe_print() {
	for (unsigned i = 0; i < 4; ++i) {
		PRINTF("%x ", result_nv[i]);
	}
	PRINTF("\r\n");
}

int main(void)
{
	chkpt_mask_init = 1;
	init();
	#if ENERGY == 0
	restore_regs();
	#endif
	chkpt_mask_init = 0;
	#if ENERGY == 1
	P1OUT &= ~BIT3;
	P1DIR |= BIT3;
	char _jit_test_src[ FIR_LENGTH << 1];
	fill_with_rand(_jit_test_src,  FIR_LENGTH << 1);
	char _jit_test_dst[ FIR_LENGTH << 1];
	PRINTF("CNT: %u %u\r\n", energy_overflow, energy_counter);
	energy_counter = 0;
	energy_overflow = 0;
	P1OUT |= BIT3;
	while (1) {
		safe_filt((_q15 *)_jit_test_src, (_q15 *)_jit_test_dst, 64);
		if (energy_counter == 0xFFFF) {
			energy_overflow = 1;
		}
		else {
			energy_counter = energy_counter + 1;
		}
	}
	#endif
	PROTECT_BEGIN();
	capybara_config_banks(0x0);
	PROTECT_END();

	while (1) {
#ifdef LOGIC
		GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_0);
		GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_0);
#endif
		for (unsigned ii = 0; ii < FILT_ITER; ++ii) {
#if SW == 0
			safe_filt(input, result_nv, SIGNAL_LENGTH);
#else
			msp_fir_q15_params firParams;
			msp_fill_q15_params fillParams;
			msp_copy_q15_params copyParams;

			fillParams.length = FIR_LENGTH*2;
			fillParams.value = 0;
			sw_fill_q15(&fillParams, circularBuffer);

			/* Initialize the copy parameter structure. */
			copyParams.length = FIR_LENGTH;

			/* Initialize the FIR parameter structure. */
			firParams.length = FIR_LENGTH;
			firParams.tapLength = COEFF_LENTH;
			firParams.coeffs = filt;
			firParams.enableCircularBuffer = true;

			/* Initialize counters. */
			uint16_t samples;
			uint16_t copyindex;
			uint16_t filterIndex;
			samples = 0;
			copyindex = 0;
			filterIndex = 2*FIR_LENGTH - COEFF_LENTH;

			/* Run FIR filter with 128 sample circular buffer. */
			while (samples < SIGNAL_LENGTH) {
				/* Copy FIR_LENGTH samples to filter input. */
				sw_copy_q15(&copyParams, &input[samples], &circularBuffer[copyindex]);

				/* Invoke the msp_fir_q15 function. */
				sw_fir_q15(&firParams, &circularBuffer[filterIndex], &result_nv[samples]);

				/* Update counters. */
				copyindex ^= FIR_LENGTH;
				filterIndex ^= FIR_LENGTH;
				samples += FIR_LENGTH;
			}
#endif
		}
#ifdef LOGIC
		GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_0);
		GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_0);
#else
		PROTECT_BEGIN();
		safe_print();
		PROTECT_END();
#endif
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
	capybara_config_banks(0x0);

#if CONT_POWER == 0
	cb_rc_t deep_discharge_status = capybara_shutdown_on_deep_discharge();
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

	GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_1);
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_1);
#else
	unsigned pc;
	__asm__ volatile ("MOV &0x4400, %0":"=m"(pc));
	PRINTF(".%x.%u\r\n", pc, _jit_0_size);
#endif
}

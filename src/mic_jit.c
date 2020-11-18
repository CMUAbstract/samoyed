#include <msp430.h>

#include <math.h>
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
#include <libapds/proximity.h>
#include <libdsp/DSPLib.h>

#include "pins.h"
#define CONT_POWER 1
#define DATA_COLLECT_MODE 0
#define SW 0
#if SW == 1
#define PROTECT_BEGIN()
#define PROTECT_END()
__nv uint8_t collecting = 0;
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
const _q15 samoyed_cmplx_twiddle_table_64_q15[DSPLIB_TABLE_OFFSET+64] = {
    0x0040, 0x0000,
    0x7FFF, 0x0000, 0x7F62, 0xF374, 0x7D8A, 0xE707, 0x7A7D, 0xDAD8,
    0x7642, 0xCF04, 0x70E3, 0xC3A9, 0x6A6E, 0xB8E3, 0x62F2, 0xAECC,
    0x5A82, 0xA57E, 0x5134, 0x9D0E, 0x471D, 0x9592, 0x3C57, 0x8F1D,
    0x30FC, 0x89BE, 0x2528, 0x8583, 0x18F9, 0x8276, 0x0C8C, 0x809E,
    0x0000, 0x8001, 0xF374, 0x809E, 0xE707, 0x8276, 0xDAD8, 0x8583,
    0xCF04, 0x89BE, 0xC3A9, 0x8F1D, 0xB8E3, 0x9592, 0xAECC, 0x9D0E,
    0xA57E, 0xA57E, 0x9D0E, 0xAECC, 0x9592, 0xB8E3, 0x8F1D, 0xC3A9,
    0x89BE, 0xCF04, 0x8583, 0xDAD8, 0x8276, 0xE707, 0x809E, 0xF374
};
msp_status sw_cmplx_bitrev_q15(const msp_cmplx_bitrev_q15_params *params, _q15 *src)
{
    uint16_t i;                     // loop counter
    uint16_t index;                 // left justified index
    uint16_t indexInc;              // index increment
    uint16_t length;                // src length
    uint16_t indexBitRev;           // index bit reversal
    uint32_t temp;                  // Temporary storage
    uint32_t *srcPtr;               // Treat complex data pairs as 32-bit data
    
    /* Initialize source pointer and length. */
    srcPtr = (uint32_t *)src;
    length = params->length;
    index = 0;
    indexInc = 2;
    
    /* Calculate index increment for left justified index. */
    while (length < 0x8000) {
        indexInc <<= 1;
        length <<= 1;
    }
        
#ifndef MSP_DISABLE_DIAGNOSTICS
    /* Check that the length is a power of two. */
    if (length != 0x8000) {
        return MSP_SIZE_ERROR;
    }
#endif //MSP_DISABLE_DIAGNOSTICS
    
    /* In-place bit-reversal using fixed table length. */
    length = params->length;
    for (i = 0; i < length; i++, index += indexInc) {
        /* Calculate bit reversed index. */
        indexBitRev = ((uint16_t)msp_cmplx_bitrev_table_ui8[index & 0xff] << 8)
            + ((uint16_t)msp_cmplx_bitrev_table_ui8[(index >> 8) & 0xff]);
        
        if (i < indexBitRev) {
            /* Swap inputs. */
            temp = srcPtr[i];
            srcPtr[i] = srcPtr[indexBitRev];
            srcPtr[indexBitRev] = temp;
        }
    }
    
    return MSP_SUCCESS;
}
#define STAGE1_STEP             (2)
#define STAGE2_STEP             (STAGE1_STEP*2)
#define STAGE3_STEP             (STAGE2_STEP*2)
#define STAGE4_STEP             (STAGE3_STEP*2)

static inline void msp_cmplx_btfly_fixed_q15(int16_t *srcA, int16_t *srcB, const _q15 *coeff);
static inline void msp_cmplx_btfly_c0_fixed_q15(int16_t *srcA, int16_t *srcB);
static inline void msp_cmplx_btfly_c1_fixed_q15(int16_t *srcA, int16_t *srcB);

/*
 * Perform in-place radix-2 DFT of the input signal using an algorithm optimized
 * for MSP430 with fixed scaling by two at each stage.
 */
msp_status sw_cmplx_fft_fixed_q15(const msp_cmplx_fft_q15_params *params, int16_t *src)
{
    int16_t i, j;                       // loop counters
    uint16_t step;                      // step size
    uint16_t length;                    // src length
    uint16_t twiddleIndex;              // twiddle table index
    uint16_t twiddleIncrement;          // twiddle table increment
    int16_t *srcPtr;                    // local source pointer
    const _q15 *twiddlePtr;             // twiddle table pointer
    msp_status status;                  // Status of the operation
    
    /* Save input length to local. */
    length = params->length;
    
    /* Bit reverse the order of the inputs. */
    if(params->bitReverse) {
        /* Create and initialize a bit reversal params structure. */
        msp_cmplx_bitrev_q15_params paramsBitRev;
        paramsBitRev.length = params->length;
        
        /* Perform bit reversal on source data. */
        status = sw_cmplx_bitrev_q15(&paramsBitRev, src);
        
        /* Check if the operation was not successful. */
        if (status !=  MSP_SUCCESS) {
            return status;
        }
    }

#ifndef MSP_DISABLE_DIAGNOSTICS
    /* Check that the length is a power of two. */
    if ((length & (length-1))) {
        return MSP_SIZE_ERROR;
    }
    
    /* Check that the provided table is the correct length. */
    if (*(uint16_t *)params->twiddleTable < length) {
        return MSP_TABLE_SIZE_ERROR;
    }
#endif //MSP_DISABLE_DIAGNOSTICS

    /* Stage 1. */
    if (STAGE1_STEP <= length) {
        for (j = 0; j < length; j += STAGE1_STEP) {
            srcPtr = src + j*2;
            msp_cmplx_btfly_c0_fixed_q15(&srcPtr[0], &srcPtr[0+STAGE1_STEP]);
        }
    }
    
    /* Stage 2. */
    if (STAGE2_STEP <= length) {
        for (j = 0; j < length; j += STAGE2_STEP) {
            srcPtr = src + j*2;
            msp_cmplx_btfly_c0_fixed_q15(&srcPtr[0], &srcPtr[0+STAGE2_STEP]);
            msp_cmplx_btfly_c1_fixed_q15(&srcPtr[2], &srcPtr[2+STAGE2_STEP]);
        }
    }
    
    /* Initialize step size, twiddle angle increment and twiddle table pointer. */
    step = STAGE3_STEP;
    twiddleIncrement = 2*(*(uint16_t*)params->twiddleTable)/STAGE3_STEP;
    twiddlePtr = &params->twiddleTable[DSPLIB_TABLE_OFFSET];
    
    /* If MPY32 is available save control context and set to fractional mode. */
#if defined(__MSP430_HAS_MPY32__)
    uint16_t ui16MPYState = MPY32CTL0;
    MPY32CTL0 = MPYFRAC | MPYDLYWRTEN;
#endif
    
    /* Stage 3 -> log2(step). */
    while (step <= length) {
        /* Reset the twiddle angle index. */
        twiddleIndex = 0;
        
        for (i = 0; i < (step/2); i++) {            
            /* Perform butterfly operations on complex pairs. */
            for (j = i; j < length; j += step) {
                srcPtr = src + j*2;
                msp_cmplx_btfly_fixed_q15(srcPtr, srcPtr + step, &twiddlePtr[twiddleIndex]);
            }
            
            /* Increment twiddle table index. */
            twiddleIndex += twiddleIncrement;
        }
        /* Double the step size and halve the increment factor. */
        step *= 2;
        twiddleIncrement = twiddleIncrement/2;
    }
    
    /* Restore MPY32 control context. */
#if defined(__MSP430_HAS_MPY32__)
    MPY32CTL0 = ui16MPYState;
#endif
    
    return MSP_SUCCESS;
}

/*
 * Simplified radix-2 butterfly operation for e^(-2*pi*(0/4)). This abstracted
 * helper function takes advantage of the fact the the twiddle coefficients are
 * positive and negative one for a multiplication by e^(-2*pi*(0/4)). The
 * following operation is performed with fixed scaling by two at each stage:
 *     A = (A + (1+0j)*B)/2
 *     B = (A - (1+0j)*B)/2
 */
static inline void msp_cmplx_btfly_c0_fixed_q15(int16_t *srcA, int16_t *srcB)
{
    int16_t tempR = CMPLX_REAL(srcB);
    int16_t tempI = CMPLX_IMAG(srcB);
    
    /* B = (A - (1+0j)*B)/2 */
    CMPLX_REAL(srcB) = __saturated_sub_q15(CMPLX_REAL(srcA), tempR) >> 1;
    CMPLX_IMAG(srcB) = __saturated_sub_q15(CMPLX_IMAG(srcA), tempI) >> 1;
    
    /* A = (A + (1+0j)*B)/2 */
    CMPLX_REAL(srcA) = __saturated_add_q15(CMPLX_REAL(srcA), tempR) >> 1;
    CMPLX_IMAG(srcA) = __saturated_add_q15(CMPLX_IMAG(srcA), tempI) >> 1;
}

/*
 * Simplified radix-2 butterfly operation for e^(-2*pi*(1/4)). This abstracted
 * helper function takes advantage of the fact the the twiddle coefficients are
 * positive and negative one for a multiplication by e^(-2*pi*(1/4)). The
 * following operation is performed with fixed scaling by two at each stage:
 *     A = (A + (0-1j)*B)/2
 *     B = (A - (0-1j)*B)/2
 */
static inline void msp_cmplx_btfly_c1_fixed_q15(int16_t *srcA, int16_t *srcB)
{
    int16_t tempR = CMPLX_REAL(srcB);
    int16_t tempI = CMPLX_IMAG(srcB);
    
    /* B = (A - (0-1j)*B)/2 */
    CMPLX_REAL(srcB) = __saturated_sub_q15(CMPLX_REAL(srcA), tempI) >> 1;
    CMPLX_IMAG(srcB) = __saturated_add_q15(CMPLX_IMAG(srcA), tempR) >> 1;
    
    /* A = (A + (0-1j)*B)/2 */
    CMPLX_REAL(srcA) = __saturated_add_q15(CMPLX_REAL(srcA), tempI) >> 1;
    CMPLX_IMAG(srcA) = __saturated_sub_q15(CMPLX_IMAG(srcA), tempR) >> 1;
}

static inline void msp_cmplx_btfly_fixed_q15(int16_t *srcA, int16_t *srcB, const _q15 *coeff)
{
    /* Load coefficients. */
    _q15 tempR = CMPLX_REAL(coeff);
    _q15 tempI = CMPLX_IMAG(coeff);
    
    /* Calculate real and imaginary parts of coeff*B. */
    __q15cmpy(&tempR, &tempI, &CMPLX_REAL(srcB), &CMPLX_IMAG(srcB));

    /* B = (A - coeff*B)/2 */
    CMPLX_REAL(srcB) = __saturated_sub_q15(CMPLX_REAL(srcA), tempR) >> 1;
    CMPLX_IMAG(srcB) = __saturated_sub_q15(CMPLX_IMAG(srcA), tempI) >> 1;
    
    /* A = (A + coeff*B)/2 */
    CMPLX_REAL(srcA) = __saturated_add_q15(CMPLX_REAL(srcA), tempR) >> 1;
    CMPLX_IMAG(srcA) = __saturated_add_q15(CMPLX_IMAG(srcA), tempI) >> 1;
}
msp_status sw_split_q15(const msp_split_q15_params *params, int16_t *src)
{
    uint16_t length;            // src length
    uint16_t tableLength;       // Coefficient table length
    uint16_t coeffOffset;       // Coefficient table increment
    int16_t aR;                 // Temp variable
    int16_t aI;                 // Temp variable
    int16_t bR;                 // Temp variable
    int16_t bI;                 // Temp variable
    int16_t cR;                 // Temp variable
    int16_t cI;                 // Temp variable
    int16_t *srcPtrK;           // Source pointer to X(k)
    int16_t *srcPtrNK;          // Source pointer to X(N-k)
    const int16_t *coeffPtr;    // Coefficient pointer
    
    /* Save input length to local. */
    length = params->length;
    
#ifndef MSP_DISABLE_DIAGNOSTICS
    /* Check that the length is a power of two. */
    if ((length & (length-1))) {
        return MSP_SIZE_ERROR;
    }
    
    /* Check that the provided table is the correct length. */
    if (*(uint16_t *)params->twiddleTable < length) {
        return MSP_TABLE_SIZE_ERROR;
    }
#endif //MSP_DISABLE_DIAGNOSTICS
    
    /* 
     * Calculate the first result bin (DC component).
     *
     *     X(N) = X(0)
     *     G(0) = 0.5*(X(0) + X*(0)) - j*0.5*(e^-j*0)*(X(0) - X*(0))
     *     G(0) = Xr(0) + Xi(0)
     */
    CMPLX_REAL(src) = CMPLX_REAL(src) + CMPLX_IMAG(src);
    CMPLX_IMAG(src) = 0;
    
    /* Initialize Src(k) and Src(N/2-k) pointers when k=1. */
    srcPtrK = src + CMPLX_INCREMENT;
    srcPtrNK = src + length - CMPLX_INCREMENT;
    
    /* Calculate coefficient table offset. */
    coeffOffset = 2;
    tableLength = *(uint16_t *)params->twiddleTable;
    while (length < tableLength) {
        coeffOffset *= 2;
        length *= 2;
    }
    
    /* Initialize coefficient pointer to index k=1. */
    coeffPtr = &params->twiddleTable[DSPLIB_TABLE_OFFSET] + coeffOffset;
    
    /*
     * Initialize length of split operations to perform. G(k) and G(N/2-k) are
     * calculated in the same loop iteration so only half of the N/2 iterations
     * are required, N/4. The last iteration where k = N/2-k will be calculated
     * separately.
     */
    length = (params->length >> 2) - 1;
    
    /* If MPY32 is available save control context and set to fractional mode. */
#if defined(__MSP430_HAS_MPY32__)
    uint16_t ui16MPYState = MPY32CTL0;
    MPY32CTL0 = MPYFRAC | MPYDLYWRTEN;
#endif
    
    /* Loop through and perform all of the split operations. */
    while(length--) {
        /* Calculate X(k) - X*(N-k) to local temporary variables. */
        bR = CMPLX_REAL(srcPtrK) - CMPLX_REAL(srcPtrNK);
        bI = CMPLX_IMAG(srcPtrK) + CMPLX_IMAG(srcPtrNK);
        
        /* B(k) = 0.5*(e^-j2*pi*k/2N)*(X(k) - X(N-k)) */
        cR = CMPLX_REAL(coeffPtr) >> 1;
        cI = CMPLX_IMAG(coeffPtr) >> 1;
        __q15cmpy(&bR, &bI, &cR, &cI);
        
        /*
         * Ar(k) = 0.5*(Xr(k) + Xr(N-k))
         * Ai(k) = 0.5*(Xi(k) - Xi(N-k))
         */
        aR = (CMPLX_REAL(srcPtrK) + CMPLX_REAL(srcPtrNK)) >> 1;
        aI = (CMPLX_IMAG(srcPtrK) - CMPLX_IMAG(srcPtrNK)) >> 1;
        
        /*
         * Gr(k) = Ar(k) + Bi(k)
         * Gi(k) = Ai(k) - Br(k)
         * Gr(N-k) = Ar(k) - Bi(k)
         * Gi(N-k) = -(Ai(k) + Br(k))
         */
        CMPLX_REAL(srcPtrK) = aR + bI;
        CMPLX_IMAG(srcPtrK) = aI - bR;
        CMPLX_REAL(srcPtrNK) = aR - bI;
        CMPLX_IMAG(srcPtrNK) = -(aI + bR);
        
        /* Update pointers. */
        srcPtrK += CMPLX_INCREMENT;
        srcPtrNK -= CMPLX_INCREMENT;
        coeffPtr += coeffOffset;
    }
    
    /* Restore MPY32 control context. */
#if defined(__MSP430_HAS_MPY32__)
    MPY32CTL0 = ui16MPYState;
#endif
    
    /* 
     * Calculate the last result bin where k = N/2-k.
     *
     *     X(k) = X(N-k)
     *     G(k) = 0.5*(X(k) + X*(k)) - j*0.5*(e^-j*pi/2)*(X(k) - X*(k))
     *     G(k) = 0.5(2*Xr(k)) - j*0.5*(-j)*(2*j*Xi(k))
     *     G(k) = Xr(k) - j*Xi(k)
     */
    CMPLX_REAL(srcPtrK) = CMPLX_REAL(srcPtrK);
    CMPLX_IMAG(srcPtrK) = -CMPLX_IMAG(srcPtrK);
    
    return MSP_SUCCESS;
}

msp_status sw_fft_fixed_q15(const msp_fft_q15_params *params, int16_t *src)
{
    msp_status status;                          // Status of the operations
    msp_split_q15_params paramsSplit;           // Split operation params
    msp_cmplx_fft_q15_params paramsCmplxFFT;    // Complex FFT params
    
    /* Initialize complex FFT params structure. */
    paramsCmplxFFT.length = params->length >> 1;
    paramsCmplxFFT.bitReverse = params->bitReverse;
    paramsCmplxFFT.twiddleTable = params->twiddleTable;
    
    /* Perform N/2 complex FFT on real source with scaling. */
    status = sw_cmplx_fft_fixed_q15(&paramsCmplxFFT, src);
    if (status !=  MSP_SUCCESS) {
        return status;
    }
    
    /* Initialize split operation params structure. */
    paramsSplit.length = params->length;
    paramsSplit.twiddleTable = params->twiddleTable;
    
    /* Perform the last stage split operation to obtain N/2 complex FFT results. */
    return sw_split_q15(&paramsSplit, src);
}
#endif

#define SEND_NUM 2
#define SAMPLE_SIZE 64
//#define WARMUP_SIZE 22
#define WARMUP_CYCLE 500
#define LOG(...)
//DSPLIB_DATA(result,MSP_ALIGN_FFT_Q15(SAMPLE_SIZE)) _q15 result[SAMPLE_SIZE];

#define MAG_SIZE SAMPLE_SIZE /2
#define MAG_ROW 1
#define MAG_COL MAG_SIZE
#define W_ROW MAG_SIZE
#define W_COL 3 + 1 // LEA Matmult does not acceept odd column number
#define RESULT_ROW MAG_ROW
#define RESULT_COL W_COL
#define b_ROW MAG_ROW
#define b_COL 3 // odd num does not work for LEA. We will not use LEA for adding
#define PROB_ROW b_ROW
#define PROB_COL b_COL

enum {QUIET, WATER_ON, DISPENSER_ON};

// magic numbers
#define WATER_ON_PACKET 0x88
#define DISPENSER_ON_PACKET 0x77 
#define QUIET_PACKET 0x66 

DSPLIB_DATA(fft_src, MSP_ALIGN_FFT_Q15(SAMPLE_SIZE)) _q15 fft_src[SAMPLE_SIZE];
DSPLIB_DATA(matmult_src0, 4) _q15 matmult_src0[MAG_ROW][MAG_COL];
DSPLIB_DATA(matmult_src1, 4) _q15 matmult_src1[W_ROW][W_COL];
DSPLIB_DATA(matmult_dst, 4) _q15 matmult_dst[RESULT_ROW][RESULT_COL];

__nv _q15 raw_data[SAMPLE_SIZE];
__nv _q15 fft_result[SAMPLE_SIZE];
__nv _q15 W[W_ROW][W_COL]= {
#include "../mic_data/set3/W.txt"
};
__nv _q15 prob[b_ROW][b_COL];
__nv _q15 b[b_ROW][b_COL] = {
#include "../mic_data/set3/b.txt"
};
__nv _q15 mag[MAG_ROW][MAG_COL];

#define RADIO_PAYLOAD_LEN 2
//#define RADIO_ON_CYCLES   60 // ~12ms (a bundle of 2-3 pkts 25 payload bytes each on Pin=0)
#define RADIO_ON_CYCLES   10 // ~12ms (a bundle of 2-3 pkts 25 payload bytes each on Pin=0)
#define RADIO_BOOT_CYCLES 60
#define RADIO_RST_CYCLES   1

typedef enum __attribute__((packed)) {
    RADIO_CMD_SET_ADV_PAYLOAD = 0,
} radio_cmd_t;

typedef struct __attribute__((packed)) {
    radio_cmd_t cmd;
    uint8_t payload[RADIO_PAYLOAD_LEN];
} radio_pkt_t;

static inline void radio_pin_init()
{
#if BOARD_MAJOR == 1 && BOARD_MINOR == 0
    GPIO(PORT_SENSE_SW, OUT) &= ~BIT(PIN_SENSE_SW);
    GPIO(PORT_SENSE_SW, DIR) |= BIT(PIN_SENSE_SW);

    GPIO(PORT_RADIO_SW, OUT) &= ~BIT(PIN_RADIO_SW);
    GPIO(PORT_RADIO_SW, DIR) |= BIT(PIN_RADIO_SW);
#elif BOARD_MAJOR == 1 && BOARD_MINOR == 1

    fxl_out(BIT_RADIO_SW | BIT_RADIO_RST);
    fxl_pull_up(BIT_CCS_WAKE);
    // SENSE_SW is present but is not electrically correct: do not use.
#elif BOARD_MAJOR == 2 && BOARD_MINOR == 0
    GPIO(PORT_RADIO_SW, OUT) &= ~BIT(PIN_RADIO_SW);
    GPIO(PORT_RADIO_SW, DIR) |= BIT(PIN_RADIO_SW);
#else // BOARD_{MAJOR,MINOR}
#error Unsupported board: do not know what pins to configure (see BOARD var)
#endif // BOARD_{MAJOR,MINOR}
}

static inline void radio_on()
{
#if (BOARD_MAJOR == 1 && BOARD_MINOR == 0) || (BOARD_MAJOR == 2 && BOARD_MINOR == 0)

#if PORT_RADIO_SW != PORT_RADIO_RST // we assume this below
#error Unexpected pin config: RAD_SW and RAD_RST not on same port
#endif // PORT_RADIO_SW != PORT_RADIO_RST

    GPIO(PORT_RADIO_SW, OUT) |= BIT(PIN_RADIO_SW) | BIT(PIN_RADIO_RST);
    GPIO(PORT_RADIO_RST, OUT) &= ~BIT(PIN_RADIO_RST);

		// KIWAN:
//    GPIO(PORT_RADIO_SW, OUT) |= BIT(PIN_RADIO_SW);
//    GPIO(PORT_RADIO_RST, OUT) |= BIT(PIN_RADIO_RST); // reset for clean(er) shutdown
//    msp_sleep(RADIO_RST_CYCLES);
//    GPIO(PORT_RADIO_RST, OUT) &= ~BIT(PIN_RADIO_RST);
#elif BOARD_MAJOR == 1 && BOARD_MINOR == 1
    // Assert reset and give it time before turning on power, to make sure that
    // radio doesn't power on while reset is (not yet) asserted and starts.
    fxl_set(BIT_RADIO_RST);
    msp_sleep(RADIO_RST_CYCLES);
    fxl_set(BIT_RADIO_SW);
    msp_sleep(RADIO_RST_CYCLES);
    fxl_clear(BIT_RADIO_RST);

#else // BOARD_{MAJOR,MINOR}
#error Unsupported board: do not know how to turn off radio (see BOARD var)
#endif // BOARD_{MAJOR,MINOR}
}

static inline void radio_off()
{
#if (BOARD_MAJOR == 1 && BOARD_MINOR == 0) || (BOARD_MAJOR == 2 && BOARD_MINOR == 0)
    GPIO(PORT_RADIO_RST, OUT) |= BIT(PIN_RADIO_RST); // reset for clean(er) shutdown
    msp_sleep(RADIO_RST_CYCLES);
    GPIO(PORT_RADIO_SW, OUT) &= ~BIT(PIN_RADIO_SW);
#elif BOARD_MAJOR == 1 && BOARD_MINOR == 1
    fxl_set(BIT_RADIO_RST); // reset for clean(er) shutdown
    msp_sleep(RADIO_RST_CYCLES);
    fxl_clear(BIT_RADIO_SW);
#else // BOARD_{MAJOR,MINOR}
#error Unsupported board: do not know how to turn on radio (see BOARD var)
#endif // BOARD_{MAJOR,MINOR}
}

static radio_pkt_t radio_pkt;

void send_radio(unsigned stat, uint8_t cnt) {
	//if (stat == WATER_ON || stat == DISPENSER_ON) {
		// burst send to make sure we receives it
		for (unsigned i = 0; i < SEND_NUM; ++i) {
			radio_pkt_t packet;

			packet.cmd = RADIO_CMD_SET_ADV_PAYLOAD;
			if (stat == WATER_ON)
				packet.payload[0] = WATER_ON_PACKET;
			else if (stat == DISPENSER_ON)
				packet.payload[0] = DISPENSER_ON_PACKET;
			else
				packet.payload[0] = QUIET_PACKET;
			packet.payload[1] = cnt;

			PRINTF("SEND BLE!\r\n");
			radio_on();
			msp_sleep(RADIO_BOOT_CYCLES); // ~15ms @ ACLK/8

			uartlink_open_tx();
			// send size + 1 (size of packet.cmd)
			uartlink_send((uint8_t *)&packet, RADIO_PAYLOAD_LEN+1);
			uartlink_close();

			// Heuristic number based on experiment
			//msp_sleep(30*RADIO_PAYLOAD_LEN);
			msp_sleep(60);
			radio_off();
		}
}	

void init()
{
	msp_watchdog_disable();
	msp_gpio_unlock();
	__enable_interrupt();
	capybara_wait_for_supply();
	capybara_config_pins();
	msp_clock_setup();

	capybara_config_banks(0xF);
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

	//i2c_setup();

	// Maliciously drain current through P1.3
	// attach a resistor to P1.3
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_DBG0);
	GPIO(PORT_DBG, DIR) |= BIT(PIN_DBG0);
	//	GPIO(PORT_DBG, OUT) |= BIT(PIN_DBG0);
	//	// and also P1.0
	//	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_0);
	//	GPIO(PORT_DBG, DIR) |= BIT(PIN_AUX_0);
	radio_pin_init();

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
	PRINTF(".%x.\r\n", pc);
#endif
#if SW == 1
//	if (collecting) {
//		// RESTOP - style
//		GPIO(PORT_MIC, DIR) |= BIT(PIN_MIC);
//		GPIO(PORT_MIC, OUT) |= BIT(PIN_MIC);
//		// Config the ADC on the comparator pin
//		P3SEL0 |= BIT2;	
//		P3SEL1 |= BIT2;	
//		// wait for the sensor to warm up
//		msp_sleep(WARMUP_CYCLE);
//	}
#endif
}

unsigned read_mic(void){
	ADC12CTL0 &= ~ADC12ENC;           // Disable conversions

	ADC12CTL1 = ADC12SHP;
	//ADC12MCTL0 = ADC12VRSEL_1 | ADC12INCH_14;
	ADC12MCTL0 = ADC12VRSEL_0 | ADC12INCH_14; 
	ADC12CTL0 |= ADC12SHT03 | ADC12ON;

	while( REFCTL0 & REFGENBUSY );

	//	REFCTL0 = REFVSEL_2 | REFON;            //Set reference voltage to 2.5
	//	REFCTL0 = REFVSEL_1 | REFON;            //Set reference voltage to 2.5

	__delay_cycles(1000);                   // Delay for Ref to settle

	ADC12CTL0 |= ADC12ENC;                  // Enable conversions
	ADC12CTL0 |= ADC12SC;                   // Start conversion
	ADC12CTL0 &= ~ADC12SC;                  // We only need to toggle to start conversion
	while (ADC12CTL1 & ADC12BUSY) ;

	unsigned output = ADC12MEM0;

	ADC12CTL0 &= ~ADC12ENC;                 // Disable conversions
	ADC12CTL0 &= ~(ADC12ON);                // Shutdown ADC12
	REFCTL0 &= ~REFON;
	return output;
}

#if SW == 1
void sw_collect_data() {
	collecting = 1;
	// enable MIC
	GPIO(PORT_MIC, DIR) |= BIT(PIN_MIC);
	GPIO(PORT_MIC, OUT) |= BIT(PIN_MIC);
	// Config the ADC on the comparator pin
	P3SEL0 |= BIT2;	
	P3SEL1 |= BIT2;	
	// wait for the sensor to warm up
	PROTECT_BEGIN();
	msp_sleep(WARMUP_CYCLE);
	PROTECT_END();

	for (unsigned i = 0; i < SAMPLE_SIZE; ++i) {
		PROTECT_BEGIN();
		unsigned mic_val = read_mic();
		PROTECT_END();
		raw_data[i] = _Q15(((float)mic_val)/4096);

		PROTECT_BEGIN();
		msp_sleep(20); // 4096 should be 1sec -- it is around 0.8 sec
		PROTECT_END();
		// 20 should be slightly over 200Hz.
	}
	// disable MIC
	GPIO(PORT_MIC, OUT) &= ~BIT(PIN_MIC);
	collecting = 0;
}
#endif

void collect_data() {
	// enable MIC
	GPIO(PORT_MIC, DIR) |= BIT(PIN_MIC);
	GPIO(PORT_MIC, OUT) |= BIT(PIN_MIC);
	// Config the ADC on the comparator pin
	P3SEL0 |= BIT2;	
	P3SEL1 |= BIT2;	

	// during warmup, useless data codes out
	//for (unsigned i = 0; i < WARMUP_SIZE; ++i) {
	//	unsigned mic_val = read_mic();
	//	msp_sleep(20); // 4096 should be 1sec -- it is around 0.8 sec
	//}
	// wait for the sensor to warm up
	msp_sleep(WARMUP_CYCLE);

	for (unsigned i = 0; i < SAMPLE_SIZE; ++i) {
		unsigned mic_val = read_mic();
		raw_data[i] = _Q15(((float)mic_val)/4096);
		msp_sleep(20); // 4096 should be 1sec -- it is around 0.8 sec
		// 20 should be slightly over 200Hz.
		GPIO(PORT_DBG, OUT) |= BIT(PIN_DBG0);
		GPIO(PORT_DBG, OUT) &= ~BIT(PIN_DBG0);
	}
	// disable MIC
	GPIO(PORT_MIC, OUT) &= ~BIT(PIN_MIC);

	//	PRINTF("RAW: ");
	//	for (unsigned i = 0; i < SAMPLE_SIZE; ++i) {
	//		PRINTF("%x ", raw_data[i]);
	//	}
	//	PRINTF("\r\n");
}

void fft() {
	// copy using dma
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "i"(raw_data));
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "i"(fft_src));
	// size in word
	DMA1SZ = SAMPLE_SIZE;
	DMA1CTL = (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);

	msp_fft_q15_params fftParams;
	fftParams.length = SAMPLE_SIZE;
	fftParams.bitReverse = true;
	fftParams.twiddleTable = msp_cmplx_twiddle_table_64_q15;
	msp_status status;
	status = msp_fft_fixed_q15(&fftParams, fft_src);

	// copy using dma
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "i"(fft_src));
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "i"(fft_result));
	// size in word
	DMA1SZ = SAMPLE_SIZE;
	DMA1CTL = (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);

	//#if DATA_COLLECT_MODE == 1
	//#if DATA_COLLECT_MODE == 0
	//	PRINTF("FFT: ");
	//	for (unsigned i = 0; i < SAMPLE_SIZE; ++i) {
	//		// get abs val
	//		if (raw_data[i] & (1 << 15)) {
	//			PRINTF("%x ", -[i]);
	//		}
	//		else {
	//			PRINTF("%x ", raw_data[i]);
	//		}
	//	}
	//	PRINTF("\r\n");
	//#endif
}

#if SW == 1
unsigned safe_pred() {
	// 1. matmult
	msp_matrix_mpy_q15_params mpyParams;

	mpyParams.srcARows = MAG_ROW;
	mpyParams.srcACols = MAG_COL;
	mpyParams.srcBRows = W_ROW;
	mpyParams.srcBCols = W_COL;

	sw_matrix_mpy_q15(&mpyParams, *mag, *W, *matmult_dst);

	// instead of copying out dst, lets add bias here as well!

	// 2. add bias
	for (unsigned i = 0; i < PROB_COL; ++i)
		prob[0][i] = __saturated_add_q15(matmult_dst[0][i], b[0][i]);

#if DATA_COLLECT_MODE == 0
	//	PRINTF("RESULT: ");
	//	for (unsigned i = 0; i < PROB_COL; ++i) {
	//		PRINTF("%x = %x + %x ||", prob[0][i], matmult_dst[0][i], b[0][i]);
	//	}
	//	PRINTF("\r\n");

	if (prob[0][0] >= prob[0][1]) {
		if (prob[0][0] >= prob[0][2]) {
			return QUIET;
		} else {
			return DISPENSER_ON;
		}
	} else {
		if (prob[0][1] >= prob[0][2]) {
			return WATER_ON;
		} else {
			return DISPENSER_ON;
		}
	}
#endif
}
#endif

unsigned pred() {
	// 0. copy using dma
	// fill src0
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "i"(mag));
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "i"(matmult_src0));
	// size in word
	DMA1SZ = MAG_ROW*MAG_COL;
	DMA1CTL = (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);

	// fill src1
	// TODO: This can be optimized: you can only load when there was a power failure
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "i"(W));
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "i"(matmult_src1));
	// size in word
	DMA1SZ = W_ROW*W_COL;
	DMA1CTL = (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);

	// 1. matmult
	msp_matrix_mpy_q15_params mpyParams;

	mpyParams.srcARows = MAG_ROW;
	mpyParams.srcACols = MAG_COL;
	mpyParams.srcBRows = W_ROW;
	mpyParams.srcBCols = W_COL;

	msp_matrix_mpy_q15(&mpyParams, *matmult_src0, *matmult_src1, *matmult_dst);

	// instead of copying out dst, lets add bias here as well!

	// 2. add bias
	// TODO: Should we use LEA? Or is this faster?
	for (unsigned i = 0; i < PROB_COL; ++i)
		prob[0][i] = __saturated_add_q15(matmult_dst[0][i], b[0][i]);

#if DATA_COLLECT_MODE == 0
	//	PRINTF("RESULT: ");
	//	for (unsigned i = 0; i < PROB_COL; ++i) {
	//		PRINTF("%x = %x + %x ||", prob[0][i], matmult_dst[0][i], b[0][i]);
	//	}
	//	PRINTF("\r\n");

	if (prob[0][0] >= prob[0][1]) {
		if (prob[0][0] >= prob[0][2]) {
			return QUIET;
		} else {
			return DISPENSER_ON;
		}
	} else {
		if (prob[0][1] >= prob[0][2]) {
			return WATER_ON;
		} else {
			return DISPENSER_ON;
		}
	}
#endif
}

int main()
{
	// Assume this region never gets an power interrupt.
	// Or is this really needed? (TODO)
	chkpt_mask_init = 1; // TODO: for now, special case for init ( But do we need this at all? )
	init();
	restore_regs();
	chkpt_mask_init = 0;

	uint8_t cnt = 0;

	while (1) {
		// collect data
#if SW == 0
		PROTECT_BEGIN();
		collect_data();
		PROTECT_END();
#else
		sw_collect_data();
#endif

		// do FFT
#if SW == 0
		PROTECT_BEGIN();
		fft();
		PROTECT_END();
#else
		msp_fft_q15_params fftParams;
		fftParams.length = SAMPLE_SIZE;
		fftParams.bitReverse = true;
		fftParams.twiddleTable = samoyed_cmplx_twiddle_table_64_q15;
		
		sw_fft_fixed_q15(&fftParams, raw_data);
#endif

#if defined(__MSP430_HAS_MPY32__)
		uint16_t ui16MPYState = MPY32CTL0;
		MPY32CTL0 = MPYFRAC_1 | MPYDLYWRTEN;
#endif
		// calc magnitude: TODO: can it also be accelerated?
		PROTECT_BEGIN();
		PRINTF("MAG: ");
		for (unsigned i = 0; i < SAMPLE_SIZE / 2; i += 1) {
#if SW == 0
			mag[0][i] = __saturated_add_q15(
					__q15mpy(CMPLX_REAL(fft_result + i*2), CMPLX_REAL(fft_result + i*2))
					, __q15mpy(CMPLX_IMAG(fft_result + i*2), CMPLX_IMAG(fft_result + i*2)));
#else
			mag[0][i] = __saturated_add_q15(
					__q15mpy(CMPLX_REAL(raw_data + i*2), CMPLX_REAL(raw_data + i*2))
					, __q15mpy(CMPLX_IMAG(raw_data + i*2), CMPLX_IMAG(raw_data + i*2)));
#endif
			// preprocessing: scale up the mag
			// mag is always positive!
			if (mag[0][i] & 0x7000) { // 0111 0000 0000 0000
				mag[0][i] = 0x7FFF;
			}
			else {
				mag[0][i] <<= 3;
			}
			PRINTF("%x ", mag[0][i]);
		}
		PRINTF("\r\n");

		PROTECT_END();
#if defined(__MSP430_HAS_MPY32__)
		MPY32CTL0 = ui16MPYState;
#endif

		// do prediction
#if SW == 0
		PROTECT_BEGIN();
		unsigned stat = pred();
		PROTECT_END();
#else
		unsigned stat = safe_pred();
#endif
		PROTECT_BEGIN();
		PRINTF("stat: %u\r\n", stat);
		PROTECT_END();

		PROTECT_BEGIN();
		send_radio(stat, cnt);
		PROTECT_END();
		cnt++;
	}

	return 0;
}

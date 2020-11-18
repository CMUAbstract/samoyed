#include "fft_sw.h"
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
    CMPLX_REAL(srcB) = (CMPLX_REAL(srcA) - tempR) >> 1;
    CMPLX_IMAG(srcB) = (CMPLX_IMAG(srcA) - tempI) >> 1;
    
    /* A = (A + (1+0j)*B)/2 */
    CMPLX_REAL(srcA) = (CMPLX_REAL(srcA) + tempR) >> 1;
    CMPLX_IMAG(srcA) = (CMPLX_IMAG(srcA) + tempI) >> 1;
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
    CMPLX_REAL(srcB) = (CMPLX_REAL(srcA) - tempI) >> 1;
    CMPLX_IMAG(srcB) = (CMPLX_IMAG(srcA) + tempR) >> 1;
    
    /* A = (A + (0-1j)*B)/2 */
    CMPLX_REAL(srcA) = (CMPLX_REAL(srcA) + tempI) >> 1;
    CMPLX_IMAG(srcA) = (CMPLX_IMAG(srcA) - tempR) >> 1;
}

static inline void msp_cmplx_btfly_fixed_q15(int16_t *srcA, int16_t *srcB, const _q15 *coeff)
{
    /* Load coefficients. */
    _q15 tempR = CMPLX_REAL(coeff);
    _q15 tempI = CMPLX_IMAG(coeff);
    
    /* Calculate real and imaginary parts of coeff*B. */
    __q15cmpy(&tempR, &tempI, &CMPLX_REAL(srcB), &CMPLX_IMAG(srcB));

    /* B = (A - coeff*B)/2 */
    CMPLX_REAL(srcB) = (CMPLX_REAL(srcA) - tempR) >> 1;
    CMPLX_IMAG(srcB) = (CMPLX_IMAG(srcA) - tempI) >> 1;
    
    /* A = (A + coeff*B)/2 */
    CMPLX_REAL(srcA) = (CMPLX_REAL(srcA) + tempR) >> 1;
    CMPLX_IMAG(srcA) = (CMPLX_IMAG(srcA) + tempI) >> 1;
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

#ifndef __FFT_SW__
#define __FFT_SW__

extern _q15 samoyed_cmplx_twiddle_table_64_q15[DSPLIB_TABLE_OFFSET+64];
msp_status sw_cmplx_bitrev_q15(const msp_cmplx_bitrev_q15_params *params, _q15 *src);
//static inline void msp_cmplx_btfly_fixed_q15(int16_t *srcA, int16_t *srcB, const _q15 *coeff);
//static inline void msp_cmplx_btfly_c0_fixed_q15(int16_t *srcA, int16_t *srcB);
//static inline void msp_cmplx_btfly_c1_fixed_q15(int16_t *srcA, int16_t *srcB);
msp_status sw_cmplx_fft_fixed_q15(const msp_cmplx_fft_q15_params *params, int16_t *src);
msp_status sw_split_q15(const msp_split_q15_params *params, int16_t *src);
msp_status sw_fft_fixed_q15(const msp_fft_q15_params *params, int16_t *src);

#endif

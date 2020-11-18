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

#include "pins.h"
#define CONT_POWER 0
#define QUICKRECALL 0
#define SW 1
//#define ADD_ITER 30
#define ADD_ITER 60

#define LOG(...)
//#define LOG(...) \
//	PROTECT_BEGIN();\
//	printf(__VA_ARGS__);\
//	PROTECT_END();

#define CONFIG_SAMPLE_SIZE 600


__nv int dst_nv[CONFIG_SAMPLE_SIZE];
__nv int src_nv[CONFIG_SAMPLE_SIZE] = {
#include "src.txt"
};

static DSPLIB_DATA(src1, 2) int src1[CONFIG_SAMPLE_SIZE];
static DSPLIB_DATA(src2, 2) int src2[CONFIG_SAMPLE_SIZE];
static DSPLIB_DATA(dest, 2) int dest[CONFIG_SAMPLE_SIZE];
static msp_status status;
void init();

msp_status sw_add_q15(const msp_add_q15_params *params, const _q15 *srcA, const _q15 *srcB, _q15 *dst)
{
    uint16_t length;

    /* Initialize the vector length. */
    length = params->length;

#ifndef MSP_DISABLE_DIAGNOSTICS
    /* Check that length parameter is a multiple of two. */
    if (length & 1) {
        return MSP_SIZE_ERROR;
    }
#endif //MSP_DISABLE_DIAGNOSTICS
    
    /* Loop through all vector elements. */
    while (length--) {
        /* Add srcA and srcB with saturation and store result. */
        *dst++ = __saturated_add_q15(*srcA++, *srcB++);
    }

    return MSP_SUCCESS;
}

void copy_and_add(msp_add_q15_params* params, int* src, int* dst) {
	memcpy(src1, src, CONFIG_SAMPLE_SIZE*sizeof(src1[0]));
	memcpy(src2, src, CONFIG_SAMPLE_SIZE*sizeof(src1[0]));
	status = msp_add_q15(&params, src1, src2, dest);
	memcpy(dst, dest, CONFIG_SAMPLE_SIZE*sizeof(src1[0]));
}

__atomic__<len:2> safe_copy_and_add(unsigned len, __inout__[len << 1] int* src) {
	// for some reason, len has to be even
	if (len % 2 == 1) {
		len = len - 1;
		// do one addition manually
		src[len] = src[len] + src[len];
	}
	// copy src to src1
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "m"(src));
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "i"(src1));
	// size in word
	DMA1SZ = len;
	DMA1CTL |= (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);
	// copy src to src2
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "m"(src));
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "i"(src2));
	// size in word
	DMA1SZ = len;
	DMA1CTL |= (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);
	msp_add_q15_params params = {.length = len};
	status = msp_add_q15(&params, src1, src2, dest);
	//memcpy(dst, dest, len*sizeof(src1[0]));
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1SA) : "i"(dest));
	__asm__ volatile ("MOVX.A %1, %0":"=m"(DMA1DA) : "m"(src));
	// size in word
	DMA1SZ = len;
	DMA1CTL |= (DMADT_1 | DMASRCINCR_3 | DMADSTINCR_3 | DMAEN | DMAREQ);

	__scaling_rule__ {
		if (len % 2 == 0) {
			safe_copy_and_add(len >> 1, src);
			safe_copy_and_add(len >> 1, src + (len >> 1));
		} else {
			safe_copy_and_add((len >> 1) + 1, src);
			safe_copy_and_add(len >> 1, src + (len >> 1) + 1);
		}
	} __scaling_rule__
	__fallback__ {
		sw_add(src, len);
	} __fallback__
}

int main()
{
	PROTECT_BEGIN();
	capybara_config_banks(0x0);
	PROTECT_END();

	while (1) {
		src_nv[CONFIG_SAMPLE_SIZE-1] = 7;
#ifdef LOGIC
		GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_0);
		GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_0);
#else
		PROTECT_BEGIN();
		PRINTF("start %u\r\n", src_nv[CONFIG_SAMPLE_SIZE-1]);
		PROTECT_END();
#endif
		for (unsigned ii = 0; ii < ADD_ITER; ++ii) {
#if QUICKRECALL == 1
			copy_and_add(&params, src_nv, dst_nv);
#elif SW == 1
			//Disable LEA
			msp_add_q15_params params = {.length = CONFIG_SAMPLE_SIZE};
			sw_add_q15(&params, src_nv, src_nv, dst_nv);
#else
			safe_copy_and_add(CONFIG_SAMPLE_SIZE, src_nv);
#endif
		}

#ifdef LOGIC
		GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_2);
		GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_2);
#else

		PROTECT_BEGIN();
		PRINTF("end %u\r\n", src_nv[CONFIG_SAMPLE_SIZE-1]);
		//PRINTF("end\r\n");
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
	PRINTF(".%x.%u\r\n", pc, _jit_0_len);
#endif
}

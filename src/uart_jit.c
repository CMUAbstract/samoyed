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

#define LOG(...)
//#define LOG(...) \
//	PROTECT_BEGIN();\
//	printf(__VA_ARGS__);\
//	PROTECT_END();

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
	PRINTF(".%x.\r\n", pc);
#endif
}

#define CONFIG_SAMPLE_SIZE 600

__nv int dst_nv[CONFIG_SAMPLE_SIZE];
__nv int src_nv[CONFIG_SAMPLE_SIZE] = {
#include "src.txt"
};

static DSPLIB_DATA(src1, 2) int src1[CONFIG_SAMPLE_SIZE];
static DSPLIB_DATA(src2, 2) int src2[CONFIG_SAMPLE_SIZE];
static DSPLIB_DATA(dest, 2) int dest[CONFIG_SAMPLE_SIZE];
static msp_status status;

void copy_and_add(msp_add_q15_params* params, int* src, int* dst) {
	memcpy(src1, src, CONFIG_SAMPLE_SIZE*sizeof(src1[0]));
	memcpy(src2, src, CONFIG_SAMPLE_SIZE*sizeof(src1[0]));
	status = msp_add_q15(&params, src1, src2, dest);
	memcpy(dst, dest, CONFIG_SAMPLE_SIZE*sizeof(src1[0]));
}

/*
void safe_copy_and_add(unsigned len, int* src, int* dst) {
	IF_ATOMIC(KNOB(len, CONFIG_SAMPLE_SIZE));
	memcpy(src1, src, len*sizeof(src1[0]));
	memcpy(src2, src, len*sizeof(src1[0]));
	msp_add_q15_params params = {.length = len};
	status = msp_add_q15(&params, src1, src2, dest);
	memcpy(dst, dest, len*sizeof(src1[0]));
	ELSE();
	safe_copy_and_add(len >> 1, src, dst);
	safe_copy_and_add(len >> 1, src + (len >> 1), dst + (len >> 1));
	END_IF();
}
*/

int main()
{
	// Assume this region never gets an power interrupt.
	// Or is this really needed? (TODO)
	chkpt_mask_init = 1; // TODO: for now, special case for init ( But do we need this at all? )
	init();
	restore_regs();
	chkpt_mask_init = 0;

	while (1) {
		dst_nv[CONFIG_SAMPLE_SIZE-1] = 7;
		PROTECT_BEGIN();
		PRINTF("start\r\n");
		PROTECT_END();

		PROTECT_BEGIN();
		PRINTF("end\r\n");
		//PRINTF("end\r\n");
		PROTECT_END();
	}

	return 0;
}

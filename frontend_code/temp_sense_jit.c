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

#ifdef CONFIG_EDB
#include <libedb/edb.h>
#else
#define ENERGY_GUARD_BEGIN()
#define ENERGY_GUARD_END()
#endif

//#include "libtemplog/templog.h"
//#include "libtemplog/print.h"

#include "pins.h"
#define CONT_POWER 0
#define QUICKRECALL 0

#define WINDOW_SIZE 3

#define LOG(...)

void init()
{
	msp_watchdog_disable();
	msp_gpio_unlock();
	__enable_interrupt();
	capybara_wait_for_supply();
	capybara_config_pins();
	msp_clock_setup();
	capybara_config_banks(0x0);

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
	PRINTF(".%x.%u\r\n", pc);
#endif
}

int average_temp() {
	int temp = 0;
	for (unsigned i = 0; i < WINDOW_SIZE; ++i) {
		temp += msp_sample_temperature();
		msp_sleep(10);
	}
	temp /= WINDOW_SIZE;
	return temp;
}

__atomic__ safe_average_temp(__output__[2] int* result) {
	int temp = 0;
	for (unsigned i = 0; i < WINDOW_SIZE; ++i) {
		temp += msp_sample_temperature();
		msp_sleep(10);
	}
	temp /= WINDOW_SIZE;
	*result = temp;
}

int main()
{
//	PROTECT_BEGIN();
//	capybara_config_banks(0x0);
//	PROTECT_END();

	while (1) {
		PROTECT_BEGIN();
		PRINTF("start\r\n");
		PROTECT_END();

#if QUICKRECALL == 1
		int temp = average_temp();
#else
		int temp;
		safe_average_temp(&temp);
#endif
		PROTECT_BEGIN();
		sleep(1000);
		PROTECT_END();

		PROTECT_BEGIN();
		PRINTF("end %u\r\n", temp);
		PROTECT_END();
	}

	return 0;

}

void __attribute__((interrupt(DMA_VECTOR)))dmaIsrHandler(void) {
	if (DMA1CTL & DMAIFG)
		DMA1CTL &= ~DMAIFG;
	__bic_SR_register_on_exit(LPM0_bits);
}

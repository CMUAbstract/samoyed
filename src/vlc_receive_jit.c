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
#include <libapds/proximity.h>
#include <libdsp/DSPLib.h>

#ifdef CONFIG_EDB
#include <libedb/edb.h>
#else
#define ENERGY_GUARD_BEGIN()
#define ENERGY_GUARD_END()
#endif

#include "pins.h"
#define CONT_POWER 1
#define THRES 100 // heuristically defined

#define LOG(...)

#define SAMPLE_SIZE 16

DSPLIB_DATA(data,MSP_ALIGN_FFT_Q15(SAMPLE_SIZE)) _q15 data[SAMPLE_SIZE];

EUSCI_B_I2C_initMasterParam params = {
	.selectClockSource = EUSCI_B_I2C_CLOCKSOURCE_SMCLK, .dataRate = EUSCI_B_I2C_SET_DATA_RATE_400KBPS,
		.byteCounterThreshold = 0, .autoSTOPGeneration = EUSCI_B_I2C_NO_AUTO_STOP};
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
	params.i2cClk = CS_getSMCLK();
	GPIO_setAsPeripheralModuleFunctionInputPin(
			GPIO_PORT_P1,
			GPIO_PIN6 + GPIO_PIN7,
			GPIO_SECONDARY_MODULE_FUNCTION
			);

//	EUSCI_B_I2C_initMasterParam param = {0};
//	param.selectClockSource = EUSCI_B_I2C_CLOCKSOURCE_SMCLK;
//	param.i2cClk = CS_getSMCLK();
//	param.dataRate = EUSCI_B_I2C_SET_DATA_RATE_400KBPS;
//	param.byteCounterThreshold = 0;
//	param.autoSTOPGeneration = EUSCI_B_I2C_NO_AUTO_STOP;
	EUSCI_B_I2C_initMaster(EUSCI_B0_BASE, &params);
	fxl_init();

	//i2c_setup();

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
	PRINTF(".%x.\r\n", pc);
#endif
}
unsigned read_light() {
	// set GPIO expander
	fxl_out(BIT_PHOTO_SW);
	fxl_set(BIT_PHOTO_SW);

	// Config the ADC on the comparator pin
	P3SEL0 |= BIT0;	
	P3SEL1 |= BIT0;	

	// ADC setup
	ADC12CTL0 &= ~ADC12ENC;           // Disable conversions
	ADC12CTL1 = ADC12SHP;
	//ADC12MCTL0 = ADC12VRSEL_1 | ADC12INCH_12;
	ADC12MCTL0 = ADC12VRSEL_0 | ADC12INCH_12;
	ADC12CTL0 |= ADC12SHT03 | ADC12ON;

	while( REFCTL0 & REFGENBUSY );

	__delay_cycles(1000);                   // Delay for Ref to settle

	ADC12CTL0 |= ADC12ENC;                  // Enable conversions
	ADC12CTL0 |= ADC12SC;                   // Start conversion
	ADC12CTL0 &= ~ADC12SC;                  // We only need to toggle to start conversion
	while (ADC12CTL1 & ADC12BUSY) ;

	unsigned output = (unsigned)ADC12MEM0;

	ADC12CTL0 &= ~ADC12ENC;                 // Disable conversions
	ADC12CTL0 &= ~(ADC12ON);                // Shutdown ADC12
	REFCTL0 &= ~REFON;

	fxl_clear(BIT_PHOTO_SW);

	return output;
}

void print_light() {
//	while (1) {
		//unsigned data[SAMPLE_SIZE];
		for (unsigned i = 0; i < SAMPLE_SIZE; ++i) {
			unsigned light = read_light();
			//data[i] = light > THRES? _Q15(1) : _Q15(0);
			light = light > THRES? light - THRES : 0;
			data[i] = _Q15(((float)(light))/4000);
			//data[i] = i % 4 == 0? _Q15(1) : _Q15(0);
			//msp_sleep(6250); // assume clk freq = 6250 (50k/8) (not sure): for time t, put 6250*t
			//msp_sleep(400); // This should be about 100ms (assuming freq = 4096 (32768/8) -- however it is 86ms. Do not know why
			msp_sleep(40); // This should be about 100ms (assuming freq = 4096 (32768/8) -- however it is 86ms. Do not know why
		}

		for (unsigned i = 0; i < SAMPLE_SIZE; ++i) {
			PRINTF("%x ", data[i]);
		}
		PRINTF("\r\n");
//	}
}

void fft() {
	// TODO: copy using dma

	msp_fft_q15_params fftParams;
	fftParams.length = SAMPLE_SIZE;
	fftParams.bitReverse = true;
	fftParams.twiddleTable = msp_cmplx_twiddle_table_256_q15;
	msp_status status;
	status = msp_fft_fixed_q15(&fftParams, data);
	PRINTF("status: %u\r\n", status);

	for (unsigned i = 0; i < SAMPLE_SIZE; ++i) {
		// get abs val
		if (data[i] & (1 << 15)) {
			int16_t newData = -((int16_t)data[i]);
			PRINTF("-%x ", -data[i]);
		}
		else {
			PRINTF("%x ", data[i]);
		}
	}
	PRINTF("\r\n");
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
	capybara_config_banks(0x1);
	PROTECT_END();

	while (1) {
		PROTECT_BEGIN();
		PRINTF("start\r\n");
		PROTECT_END();

		PROTECT_BEGIN();
		print_light();
		PROTECT_END();

		PROTECT_BEGIN();
		fft();
		PROTECT_END();
	}

	return 0;
}

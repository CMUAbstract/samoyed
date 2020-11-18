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

#include "pins.h"
#define CONT_POWER 0
#define QUICKRECALL 0

#define RADIO_PAYLOAD_LEN 4 // 16 does not work, 4 works (24 is max)
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
static uint8_t data[RADIO_PAYLOAD_LEN];
void send_radio(uint8_t* payload, size_t size) {
	radio_pkt_t packet;

	packet.cmd = RADIO_CMD_SET_ADV_PAYLOAD;
	memcpy(packet.payload, payload, size);

	radio_on();
	msp_sleep(RADIO_BOOT_CYCLES); // ~15ms @ ACLK/8

	uartlink_open_tx();
	// send size + 1 (size of packet.cmd)
	uartlink_send((uint8_t *)&packet, size+1);
	uartlink_close();

	// Heuristic number based on experiment
//	msp_sleep(30*size);
//	radio_off();
}	

__atomic__<size:1> safe_send_radio(__input__[size] uint8_t* payload, size_t size) {
	radio_pkt_t packet;

	packet.cmd = RADIO_CMD_SET_ADV_PAYLOAD;
	memcpy(packet.payload, payload, size);

	radio_on();
	msp_sleep(RADIO_BOOT_CYCLES); // ~15ms @ ACLK/8

	uartlink_open_tx();
	uartlink_send((uint8_t *)&packet, size+1);
	uartlink_close();

	// Heuristic number based on experiment
	msp_sleep(30*size);
	radio_off();
	__scaling_rule__ {
		safe_send_radio(payload, size >> 1);
		safe_send_radio(payload + (size >> 1), size >> 1);
	} __scaling_rule__
}
/*
	 void safe_send_radio(uint8_t* payload, size_t size) {
	 IF_ATOMIC(KNOB(size, RADIO_PAYLOAD_LEN));
	 radio_pkt_t packet;

	 packet.cmd = RADIO_CMD_SET_ADV_PAYLOAD;
	 memcpy(packet.payload, payload, size);

	 radio_on();
	 msp_sleep(RADIO_BOOT_CYCLES); // ~15ms @ ACLK/8

	 uartlink_open_tx();
	 uartlink_send((uint8_t *)&packet, size+1);
	 uartlink_close();

// Heuristic number based on experiment
msp_sleep(30*size);
radio_off();
ELSE();
safe_send_radio(payload, size >> 1);
safe_send_radio(payload + (size >> 1), size >> 1);
END_IF();
}*/
int main()
{
	//	PROTECT_BEGIN();
	//	capybara_config_banks(0x6);
	//	PROTECT_END();
	unsigned cnt = 0;

	while (1) {
		PROTECT_BEGIN();
		PRINTF("start\r\n");
		PROTECT_END();

		while (1) {
			for (int j = 0; j < RADIO_PAYLOAD_LEN; ++j) {
				data[j] = 0x77;
			}

#if QUICKRECALL == 1
			send_radio(&data, RADIO_PAYLOAD_LEN);
#else
			safe_send_radio(&data, RADIO_PAYLOAD_LEN);
#endif
			// translated code end

			PROTECT_BEGIN();
			PRINTF("end %u\r\n", cnt);
			PROTECT_END();
			cnt++;
		}
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
	capybara_config_banks(0xf);
#if CONT_POWER == 0
	cb_rc_t deep_discharge_status = capybara_shutdown_on_deep_discharge();

	if (deep_discharge_status == CB_ERROR_ALREADY_DEEPLY_DISCHARGED)
		capybara_shutdown();
#endif

	INIT_CONSOLE();

#if BOARD_MAJOR == 1 && BOARD_MINOR == 1
	i2c_setup();
	fxl_init();
#endif // BOARD == v1.1

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
	PRINTF(".%x.%u\r\n", pc, _jit_0_size);
#endif
}


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
//#include <libmspmath/msp-math.h>
#include <libcapybara/capybara.h>
#include <libcapybara/power.h>
#include <libjit/jit.h>
#include <loopcnt.h>

#define ENERGY_GUARD_BEGIN()
#define ENERGY_GUARD_END()

#include "pins.h"

// #define SHOW_PROGRESS_ON_LED
#include <stdint.h>

//#define NUM_BUCKETS 256 // must be a power of 2
#define NUM_BUCKETS 128 // must be a power of 2
//#define NUM_BUCKETS 64 // must be a power of 2
#define MAX_RELOCATIONS 8
#define CONT_POWER 0
typedef uint16_t value_t;
typedef uint16_t hash_t;
typedef uint16_t fingerprint_t;
typedef uint16_t index_t; // bucket index

#define NUM_KEYS (NUM_BUCKETS / 4) // shoot for 25% occupancy
#define INIT_KEY 0x1

static void init_hw()
{
	msp_watchdog_disable();
	msp_gpio_unlock();
	msp_clock_setup();
}
void print_filter(fingerprint_t *filter)
{
    unsigned i;
	BLOCK_PRINTF_BEGIN();
	for (i = 0; i < NUM_BUCKETS; ++i) {
		BLOCK_PRINTF("%04x ", filter[i]);
		if (i > 0 && (i + 1) % 8 == 0){
			BLOCK_PRINTF("\r\n");
		}
	}
	BLOCK_PRINTF_END();
}

static hash_t djb_hash(uint8_t* data, unsigned len)
{
	uint32_t hash = 5381;
	unsigned int i;

	for(i = 0; i < len; data++, i++)
		hash = ((hash << 5) + hash) + (*data);

	return hash & 0xFFFF;
}

static index_t hash_fp_to_index(fingerprint_t fp)
{
	hash_t hash = djb_hash((uint8_t *)&fp, sizeof(fingerprint_t));
	return hash & (NUM_BUCKETS - 1); // NUM_BUCKETS must be power of 2
}

static index_t hash_key_to_index(value_t fp)
{
	hash_t hash = djb_hash((uint8_t *)&fp, sizeof(value_t));
	return hash & (NUM_BUCKETS - 1); // NUM_BUCKETS must be power of 2
}

static fingerprint_t hash_to_fingerprint(value_t key)
{
	return djb_hash((uint8_t *)&key, sizeof(value_t));
}

static value_t generate_key(value_t prev_key)
{
	// insert pseufo-random integers, for testing
	// If we use consecutive ints, they hash to consecutive DJB hashes...
	// NOTE: we are not using rand(), to have the sequence available to verify
	// that that are no false negatives (and avoid having to save the values).
	return (prev_key + 1) * 17;
}

static bool insert(fingerprint_t *filter, value_t key)
{
	fingerprint_t fp1, fp2, fp_victim, fp_next_victim;
	index_t index_victim, fp_hash_victim;
	unsigned relocation_count = 0;

	fingerprint_t fp = hash_to_fingerprint(key);

	index_t index1 = hash_key_to_index(key);

	index_t fp_hash = hash_fp_to_index(fp);
	index_t index2 = index1 ^ fp_hash;

	LOG("insert: key %04x fp %04x h %04x i1 %u i2 %u\r\n",
			key, fp, fp_hash, index1, index2);

	fp1 = filter[index1];
	LOG("insert: fp1 %04x\r\n", fp1);
	if (!fp1) { // slot 1 is free
		filter[index1] = fp;
	} else {
		fp2 = filter[index2];
		LOG("insert: fp2 %04x\r\n", fp2);
		if (!fp2) { // slot 2 is free
			filter[index2] = fp;
		} else { // both slots occupied, evict
			if (rand() & 0x80) { // don't use lsb because it's systematic
				index_victim = index1;
				fp_victim = fp1;
			} else {
				index_victim = index2;
				fp_victim = fp2;
			}

			LOG("insert: evict [%u] = %04x\r\n", index_victim, fp_victim);
			filter[index_victim] = fp; // evict victim

			do { // relocate victim(s)
				fp_hash_victim = hash_fp_to_index(fp_victim);
				index_victim = index_victim ^ fp_hash_victim;

				fp_next_victim = filter[index_victim];
				filter[index_victim] = fp_victim;

				LOG("insert: moved %04x to %u; next victim %04x\r\n",
						fp_victim, index_victim, fp_next_victim);

				fp_victim = fp_next_victim;
			} while (fp_victim && ++relocation_count < MAX_RELOCATIONS);

			if (fp_victim) {
				//PRINTF("insert: lost fp %04x\r\n", fp_victim);
				return false;
			}
		}
	}

	return true;
}

static bool lookup(fingerprint_t *filter, value_t key)
{
	fingerprint_t fp = hash_to_fingerprint(key);

	index_t index1 = hash_key_to_index(key);

	index_t fp_hash = hash_fp_to_index(fp);
	index_t index2 = index1 ^ fp_hash;

	LOG("lookup: key %04x fp %04x h %04x i1 %u i2 %u\r\n",
			key, fp, fp_hash, index1, index2);

	return filter[index1] == fp || filter[index2] == fp;
}

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
#if CONT_POWER == 0
	// Setup deep discharge shutdown interrupt before reconfiguring banks,
	// so that if we deep discharge during reconfiguration, we still at
	// least try to avoid cold start. NOTE: this is controversial./
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
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_DBG0);
	GPIO(PORT_DBG, DIR) |= BIT(PIN_DBG0);
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
#else
	unsigned pc;
	__asm__ volatile ("MOV &0x4400, %0":"=m"(pc));
//	if (!chkptEnd) {
//		if (chkptStart)
//			PRINTF("started!\r\n");
//		else
//			PRINTF("err:%x\r\n", pc);
//	}
	PRINTF(".%x.\r\n", pc);
//	chkptStart = 0;
//	chkptEnd = 0;
#endif
}

__nv volatile fingerprint_t filter[NUM_BUCKETS];
int main()
{
	chkpt_mask_init = 1; // TODO: for now, special case for init ( But do we need this at all? )
	init();
	restore_regs();
	chkpt_mask_init = 0;

	unsigned i;
	value_t key;

	while (1) {
		;
#ifdef LOGIC
		GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_0);
		GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_0);
#else
		PROTECT_BEGIN();
		PRINTF("s\r\n");
		PROTECT_END();
#endif
		unsigned inserts, members;

		for (unsigned ii = 0; ii < LOOP_CF; ++ii) {
			for (i = 0; i < NUM_BUCKETS; ++i)
				filter[i] = 0;

			key = INIT_KEY;
			inserts = 0;
			for (i = 0; i < NUM_KEYS; ++i) {
				key = generate_key(key);
				bool success = insert(filter, key);
				LOG("insert: key %04x success %u\r\n", key, success);
				if (!success) {
					;
					PROTECT_BEGIN();
					PRINTF("insert failed\r\n");
					//PRINTF("insert: key %04x failed\r\n", key);
					PROTECT_END();
				}

				inserts += success;

			}
			LOG("inserts/total: %u/%u\r\n", inserts, NUM_KEYS);

			key = INIT_KEY;
			members = 0;
			for (i = 0; i < NUM_KEYS; ++i) {
				key = generate_key(key);
				bool member = lookup(filter, key);
				LOG("lookup: key %04x member %u\r\n", key, member);
				if (!member) {
					fingerprint_t fp = hash_to_fingerprint(key);
					//PRINTF("lookup: key %04x fp %04x not member\r\n", key, fp);
				}
				members += member;
			}
			LOG("members/total: %u/%u\r\n", members, NUM_KEYS);
		}
		;
#ifdef LOGIC
		GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_2);
		GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_2);
#else
		PROTECT_BEGIN();
		PRINTF("%u %u %u\r\n", inserts, members, NUM_KEYS);
		PROTECT_END();
#endif
	}

	return 0;
}

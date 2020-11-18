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
#include <loopcnt.h>

#define CONT_POWER 0

#include "pins.h"
#define TEST_SAMPLE_DATA
// #define SHOW_PROGRESS_ON_LED

#define NIL 0 // like NULL, but for indexes, not real pointers

#define DICT_SIZE         512
#define BLOCK_SIZE         64

#define NUM_LETTERS_IN_SAMPLE        2
#define LETTER_MASK             0x00FF
#define LETTER_SIZE_BITS             8
#define NUM_LETTERS (LETTER_MASK + 1)
typedef unsigned index_t;
typedef unsigned letter_t;
typedef unsigned sample_t;
// We need to mask capy interrupt before doing
// mandatory checkpoint. This is because this checkpoint SHOULD
// always succeed. There is no protection if this fails
// (even with double buffering, because the control
// flow should never go back)
// Worst case, the checkpoint will finish (hopefully, because
// if interrupt fired in here, we know we had more than threshold
// voltage when entering the checkpoint -- this can be safely
// assumed by tuning the threshold)
// but we will enter cold start.
#define PROTECT_START() \
	COMP_VBANK(INT) &= ~COMP_VBANK(IE);\
	chkpt_mask = 1;\
	checkpoint();\
	COMP_VBANK(INT) |= COMP_VBANK(IE);


#define PROTECT_END() \
	chkpt_mask = 0;

#define LOG(...)
//#define LOG(...) \
//	PROTECT_START();\
//	printf(__VA_ARGS__);\
//	PROTECT_END();

//__attribute__((section("__interrupt_vector_timer0_b1"),aligned(2)))
//void(*__vector_timer0_b1)(void) = TimerB1_ISR;

// NOTE: can't use pointers, since need to ChSync, etc
typedef struct _node_t {
	letter_t letter; // 'letter' of the alphabet
	index_t sibling; // this node is a member of the parent's children list
	index_t child;   // link-list of children
} node_t;

typedef struct _dict_t {
	node_t nodes[DICT_SIZE];
	unsigned node_count;
} dict_t;

typedef struct _log_t {
	index_t data[BLOCK_SIZE];
	unsigned count;
	unsigned sample_count;
} log_t;
void print_log(log_t *log)
{
	PRINTF("rate: samples/block: %u/%u\r\n",
			log->sample_count, log->count);
	//unsigned i;
	//BLOCK_PRINTF_BEGIN();
	//BLOCK_PRINTF("rate: samples/block: %u/%u\r\n",
	//		log->sample_count, log->count);
	//BLOCK_PRINTF("compressed block:\r\n");
	//for (i = 0; i < log->count; ++i) {
	//	BLOCK_PRINTF("%04x ", log->data[i]);
	//	if (i > 0 && ((i + 1) & (8 - 1)) == 0){
	//	}
	//	BLOCK_PRINTF("\r\n");
	//}
	//if ((log->count & (8 - 1)) != 0){
	//}
	//BLOCK_PRINTF("\r\n");
	//BLOCK_PRINTF_END();
}

static sample_t acquire_sample(letter_t prev_sample)
{
	//letter_t sample = rand() & 0x0F;
	letter_t sample = (prev_sample + 1) & 0x03;
	return sample;
}

void init_dict(dict_t *dict)
{
	letter_t l;

	LOG("init dict\r\n");
	dict->node_count = 0;

	for (l = 0; l < NUM_LETTERS; ++l) {
		node_t *node = &dict->nodes[l];
		node->letter = l;
		node->sibling = 0;
		node->child = 0;

		dict->node_count++;
		LOG("init dict: node count %u\r\n", dict->node_count);
	}
}

index_t find_child(letter_t letter, index_t parent, dict_t *dict)
{
	node_t *parent_node = &dict->nodes[parent];

	LOG("find child: l %u p %u c %u\r\n", letter, parent, parent_node->child);

	if (parent_node->child == NIL) {
		LOG("find child: not found (no children)\r\n");
		return NIL;
	}

	index_t sibling = parent_node->child;
	while (sibling != NIL) {
		node_t *sibling_node = &dict->nodes[sibling];

		LOG("find child: l %u, s %u l %u s %u\r\n", letter,
				sibling, sibling_node->letter, sibling_node->sibling);

		if (sibling_node->letter == letter) { // found
			LOG("find child: found %u\r\n", sibling);
			return sibling;
		} else {
			sibling = sibling_node->sibling;
		}
	}

	LOG("find child: not found (no match)\r\n");
	return NIL; 
}

void add_node(letter_t letter, index_t parent, dict_t *dict)
{
	if (dict->node_count == DICT_SIZE) {
		PRINTF("add node: table full\r\n");
		while(1); // bail for now
	}
	// Initialize the new node
	node_t *node = &dict->nodes[dict->node_count];

	node->letter = letter;
	node->sibling = NIL;
	node->child = NIL;

	index_t node_index = dict->node_count++;

	index_t child = dict->nodes[parent].child;

	LOG("add node: i %u l %u, p: %u pc %u\r\n",
			node_index, letter, parent, child);

	if (child) {
		LOG("add node: is sibling\r\n");

		// Find the last sibling in list
		index_t sibling = child;
		node_t *sibling_node = &dict->nodes[sibling];
		while (sibling_node->sibling != NIL) {
			LOG("add node: sibling %u, l %u s %u\r\n",
					sibling, letter, sibling_node->sibling);
			sibling = sibling_node->sibling;
			sibling_node = &dict->nodes[sibling];
		}
		// Link-in the new node
		LOG("add node: last sibling %u\r\n", sibling);
		dict->nodes[sibling].sibling = node_index;
	} else {
		LOG("add node: is only child\r\n");
		dict->nodes[parent].child = node_index;
	}
}

void append_compressed(index_t parent, log_t *log)
{
	LOG("append comp: p %u cnt %u\r\n", parent, log->count);
	log->data[log->count++] = parent;
}

__nv unsigned debug_cntr = 0;
void init()
{
	msp_watchdog_disable();
	msp_gpio_unlock();
	__enable_interrupt();
	capybara_wait_for_supply();
	capybara_config_pins();
	msp_clock_setup();

	capybara_config_banks(0x0);
	P3DIR &= ~PIN4;
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
	PRINTF(".%x.%u\r\n", pc, debug_cntr);
//	chkptStart = 0;
//	chkptEnd = 0;
#endif
}

int main()
{
	// if chkpt_ever_taken,
	// switch to volatile RAM to protect FRAM stack on boot
	// This is now done by the python script

	// Assume this region never gets an power interrupt.
	// Or is this really needed? (TODO)
	chkpt_mask_init = 1; // TODO: for now, special case for init ( But do we need this at all? )
	init();
	restore_regs();
	chkpt_mask_init = 0;

	static volatile __nv dict_t dict;
	static volatile __nv log_t log;
	//	test_func();
	while (1) {
#ifdef LOGIC
		GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_0);
		GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_0);
#else
		PROTECT_START();
		PRINTF("start: %u\r\n", debug_cntr);
		PROTECT_END();
#endif
		for (unsigned ii = 0; ii < LOOP_CEM; ++ii) {
			log.count = 0;
			log.sample_count = 0;
			init_dict(&dict);

			// Initialize the pointer into the dictionary to one of the root nodes
			// Assume all streams start with a fixed prefix ('0'), to avoid having
			// to letterize this out-of-band sample.
			letter_t letter = 0;

			unsigned letter_idx = 0;
			index_t parent, child;
			sample_t sample, prev_sample = 0;

			log.sample_count = 1; // count the initial sample (see above)
			log.count = 0; // init compressed counter

			while (1) {
				child = (index_t)letter; // relyes on initialization of dict
				LOG("compress: parent %u\r\n", child); // naming is odd due to loop

				if (letter_idx == 0) {
					sample = acquire_sample(prev_sample);
					prev_sample = sample;
				}

				LOG("letter index: %u\r\n", letter_idx);
				letter_idx++;
				if (letter_idx == NUM_LETTERS_IN_SAMPLE)
					letter_idx = 0;
				do {
					unsigned letter_idx_tmp = (letter_idx == 0) ? NUM_LETTERS_IN_SAMPLE : letter_idx - 1; 

					unsigned letter_shift = LETTER_SIZE_BITS * letter_idx_tmp;
					letter = (sample & (LETTER_MASK << letter_shift)) >> letter_shift;
					LOG("letterize: sample %x letter %x (%u)\r\n",
							sample, letter, letter);

					log.sample_count++;
					LOG("sample count: %u\r\n", log.sample_count);
					parent = child;
					child = find_child(letter, parent, &dict);
				} while (child != NIL);

				append_compressed(parent, &log);
				add_node(letter, parent, &dict);

				if (log.count == BLOCK_SIZE) {
					break;
				}
			}
		}
#ifndef LOGIC
		PROTECT_START();
		print_log(&log);
		PROTECT_END();
#endif
#ifdef LOGIC
		//PROTECT_START();
	//BLOCK_PRINTF_BEGIN();
	//BLOCK_PRINTF("rate: samples/block: %u/%u\r\n",
	//		log->sample_count, log->count);
	//BLOCK_PRINTF("compressed block:\r\n");
	//for (i = 0; i < log->count; ++i) {
	//	BLOCK_PRINTF("%04x ", log->data[i]);
	//	if (i > 0 && ((i + 1) & (8 - 1)) == 0){
	//	}
	//	BLOCK_PRINTF("\r\n");
	//}
	//if ((log->count & (8 - 1)) != 0){
	//}
		//GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_2);
		//GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_2);
		//PROTECT_END();
#endif
		debug_cntr++;
	}
	return 0;
}

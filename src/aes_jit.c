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
#include <libaes/TI_aes_128_encr_only.h>

#include "pins.h"
#define CONT_POWER 0
#define QUICKRECALL 0
#define SW 1

#define LOG(...)
#define DATA_SIZE 2048 // max 2048
#define AES_CNT 5

void init();
__nv char key[32] = "qazwsxedcrfvtgbyhnujioplkqwertgf";
__nv char plaintext[DATA_SIZE];
__nv char ciphertext[DATA_SIZE];

__nv unsigned int _jit_0_len = 65535;
int _jit_safe_0(char * key, char * plaintext, char * ciphertext, unsigned int len)  {
	unsigned int len_bak = len;
	if (len_bak <= _jit_0_len && !_jit_no_progress) {
;
	;
	;
	;
	;

	// reset aes
	AESACTL0=AESSWRST;
	// set key
	AES256_setCipherKey(AES256_BASE, key, AES256_KEYLENGTH_256BIT);
	// DMA trigger mode
	AESACTL0 |= AESCMEN;

	// DMA0 set
	// Trigger select: DMA0 - AES0 / DMA1 - AES1
	DMACTL0 = (DMA0TSEL_11 | DMA1TSEL_12);
	// DMA0 source: AESADOUT, dest: ciphertext
	// size: num_blocks*8 words, single transfer mode (default)
	DMA0CTL = DMADT_0 | DMALEVEL | DMASRCINCR_0 | DMADSTINCR_3;
	DMA0SA = &AESADOUT;
	DMA0SZ = len > 2048 ? 1024 : len >> 1;
	DMA0CTL |= DMAEN;
	// DMA1 source: plaintext, dest: AESADIN
	// size: num_blocks*8 words, single transfer mode (default)
	DMA1CTL = DMADT_0 | DMALEVEL | DMASRCINCR_3 | DMADSTINCR_0;
	DMA1DA = &AESADIN;
	DMA1SZ = len > 2048 ? 1024 : len >> 1;
	DMA1CTL |= DMAEN;

	char* src = plaintext;
	char* dst = ciphertext;
	while (len > 0) {
		// at max, 2048 byte can be ciphered
		unsigned newLen = len > 2048 ? 2048 : len;
		DMA1SA = src;
		DMA0DA = dst;
		// num_blocks ( each block is 16 byte)
		// AESBLKCNTx is 8bit -> max 256
		// That means len is max 256 << 4 = 2046
		AESACTL1 = newLen >> 4;
		while(!(DMA0CTL & DMAIFG));

		src += newLen;
		dst += newLen;
		len -= newLen;
	}
		if (_jit_bndMayNeedUpdate) {
			_jit_0_len = len_bak;
			_jit_bndMayNeedUpdate = 0;
		}
		return 1;
	}
	return 0;
}
void safe_aes_run(char* key, char* plaintext, char* ciphertext, unsigned len) {
		int success = 0;
	if (plaintext == ciphertext) {
		if (len <= _jit_0_len) {
			recursiveUndoLog(plaintext,  len);
		}
	}
	if (key == ciphertext) {
		if (len <= _jit_0_len) {
			recursiveUndoLog(key,  32);
		}
	}
	undoLogPtr = undoLogPtr_tmp;
	undoLogCnt = undoLogCnt_tmp;
	PROTECT_BEGIN();
	undoLogPtr = undoLogPtr_tmp;
	undoLogCnt = undoLogCnt_tmp;
	success = _jit_safe_0(key, plaintext, ciphertext, len);
	PROTECT_END();
	if (!success) {
;
	safe_aes_run(key, plaintext, ciphertext, len >> 1);
	safe_aes_run(key, plaintext + (len >> 1), ciphertext + (len >> 1), len >> 1);
	}
;
}

/*
	 void safe_aes_run(char* key, char* plaintext, char* ciphertext, unsigned len) {
	 IF_ATOMIC(KNOB(len, DATA_SIZE));
// reset aes
AESACTL0=AESSWRST;
// set key
AES256_setCipherKey(AES256_BASE, key, AES256_KEYLENGTH_256BIT);
// DMA trigger mode
AESACTL0 |= AESCMEN;

// DMA0 set
// Trigger select: DMA0 - AES0 / DMA1 - AES1
DMACTL0 = (DMA0TSEL_11 | DMA1TSEL_12);
// DMA0 source: AESADOUT, dest: ciphertext
// size: num_blocks*8 words, single transfer mode (default)
DMA0CTL = DMADT_0 | DMALEVEL | DMASRCINCR_0 | DMADSTINCR_3;
DMA0SA = &AESADOUT;
DMA0SZ = len > 2048 ? 1024 : len >> 1;
DMA0CTL |= DMAEN;
// DMA1 source: plaintext, dest: AESADIN
// size: num_blocks*8 words, single transfer mode (default)
DMA1CTL = DMADT_0 | DMALEVEL | DMASRCINCR_3 | DMADSTINCR_0;
DMA1DA = &AESADIN;
DMA1SZ = len > 2048 ? 1024 : len >> 1;
DMA1CTL |= DMAEN;

char* src = plaintext;
char* dst = ciphertext;
while (len > 0) {
// at max, 2048 byte can be ciphered
unsigned newLen = len > 2048 ? 2048 : len;
DMA1SA = src;
DMA0DA = dst;
// num_blocks ( each block is 16 byte)
// AESBLKCNTx is 8bit -> max 256
// That means len is max 256 << 4 = 2046
AESACTL1 = newLen >> 4;
while(!(DMA0CTL & DMAIFG));

src += newLen;
dst += newLen;
len -= newLen;
}
ELSE();
safe_aes_run(key, plaintext, ciphertext, len >> 1);
safe_aes_run(key, plaintext + (len >> 1), ciphertext + (len >> 1), len >> 1);
END_IF();
}
 */
void aes_run(char* key, char* plaintext, char* ciphertext, unsigned len) {
	// reset aes
	AESACTL0=AESSWRST;
	// set key
	AES256_setCipherKey(AES256_BASE, key, AES256_KEYLENGTH_256BIT);
	// DMA trigger mode
	AESACTL0 |= AESCMEN;

	// DMA0 set
	// Trigger select: DMA0 - AES0 / DMA1 - AES1
	DMACTL0 = (DMA0TSEL_11 | DMA1TSEL_12);
	// DMA0 source: AESADOUT, dest: ciphertext
	// size: num_blocks*8 words, single transfer mode (default)
	DMA0CTL = DMADT_0 | DMALEVEL | DMASRCINCR_0 | DMADSTINCR_3;
	DMA0SA = &AESADOUT;
	DMA0SZ = len > 2048 ? 1024 : len >> 1;
	DMA0CTL |= DMAEN;
	// DMA1 source: plaintext, dest: AESADIN
	// size: num_blocks*8 words, single transfer mode (default)
	DMA1CTL = DMADT_0 | DMALEVEL | DMASRCINCR_3 | DMADSTINCR_0;
	DMA1DA = &AESADIN;
	DMA1SZ = len > 2048 ? 1024 : len >> 1;
	DMA1CTL |= DMAEN;

	char* src = plaintext;
	char* dst = ciphertext;
	while (len > 0) {
		// at max, 2048 byte can be ciphered
		unsigned newLen = len > 2048 ? 2048 : len;
		DMA1SA = src;
		DMA0DA = dst;
		// num_blocks ( each block is 16 byte)
		// AESBLKCNTx is 8bit -> max 256
		// That means len is max 256 << 4 = 2046
		AESACTL1 = newLen >> 4;
		while(!(DMA0CTL & DMAIFG));

		src += newLen;
		dst += newLen;
		len -= newLen;
	}
}

int main()
{
	chkpt_mask_init = 1;
	init();
	#if ENERGY == 0
	restore_regs();
	#endif
	chkpt_mask_init = 0;
	#if ENERGY == 1
	P1OUT &= ~BIT3;
	P1DIR |= BIT3;
	char _jit_test_plaintext[ 16];
	fill_with_rand(_jit_test_plaintext,  16);
	char _jit_test_key[ 32];
	fill_with_rand(_jit_test_key,  32);
	char _jit_test_ciphertext[ 16];
	PRINTF("CNT: %u %u\r\n", energy_overflow, energy_counter);
	energy_counter = 0;
	energy_overflow = 0;
	PROTECT_BEGIN();
	capybara_config_banks(0xF);
	PROTECT_END();
	P1OUT |= BIT3;
	while (1) {
		safe_aes_run((char *)_jit_test_key, (char *)_jit_test_plaintext, (char *)_jit_test_ciphertext, 16);
		if (energy_counter == 0xFFFF) {
			energy_overflow = 1;
		}
		else {
			energy_counter = energy_counter + 1;
		}
	}
	#endif

	for (unsigned i = 0; i < DATA_SIZE; ++i) {
		plaintext[i] = 0x77;
	}
	while (1) {
#ifdef LOGIC
		GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_0);
		GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_0);
#else
		;
		PROTECT_BEGIN();
		PRINTF("start\r\n");
		PROTECT_END();
#endif

		for (unsigned ii = 0; ii < AES_CNT; ++ii) {
#if QUICKRECALL == 1
			aes_run(key, plaintext, ciphertext, DATA_SIZE);
#elif SW == 1
			//	for (unsigned i = 0; i < 16; ++i) {
			//		ciphertext[i] = plaintext[i];
			//	}
			for (unsigned i = 0; i < DATA_SIZE / 16; ++i) {
				aes_encrypt(plaintext+i*16, key);
			}

#else
			safe_aes_run(key, plaintext, ciphertext, DATA_SIZE);
#endif
		}
#ifdef LOGIC
		GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_2);
		GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_2);
#else
		for (unsigned i = 0; i < 16; ++i) {
			;
			PROTECT_BEGIN();
			PRINTF("%x ", ciphertext[i]);
			PROTECT_END();
			if (i % 4 == 3) {
				;
				PROTECT_BEGIN();
				PRINTF("\r\n");
				PROTECT_END();
			}
		}
		;
		PROTECT_BEGIN();
		PRINTF("end\r\n");
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
	PRINTF(".%x. %u \r\n", pc, _jit_0_len);
#endif
}

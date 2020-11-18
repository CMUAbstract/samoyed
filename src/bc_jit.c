#include <msp430.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef LOGIC
#define LOG(...)
#define LOG2(...)
#define PRINTF(...)
#define EIF_PRINTF(...)
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
#include "loopcnt.h"

#define CONT_POWER 0
#define SEED 4L
#define ITER 100
#define CHAR_BIT 8
__nv static char bits[256] =
{
      0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,  /* 0   - 15  */
      1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,  /* 16  - 31  */
      1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,  /* 32  - 47  */
      2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  /* 48  - 63  */
      1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,  /* 64  - 79  */
      2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  /* 80  - 95  */
      2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  /* 96  - 111 */
      3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,  /* 112 - 127 */
      1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,  /* 128 - 143 */
      2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  /* 144 - 159 */
      2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  /* 160 - 175 */
      3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,  /* 176 - 191 */
      2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  /* 192 - 207 */
      3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,  /* 208 - 223 */
      3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,  /* 224 - 239 */
      4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8   /* 240 - 255 */
};

#include "pins.h"

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
//	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_1);
#else
	unsigned pc;
	__asm__ volatile ("MOV &0x4400, %0":"=m"(pc));
	PRINTF(".%x.\r\n", pc);
#endif
}

int btbl_bitcnt(uint32_t x)
{
      int cnt = bits[ ((char *)&x)[0] & 0xFF ];

      if (0L != (x >>= 8))
            cnt += btbl_bitcnt(x);
      return cnt;
}
int bit_count(uint32_t x)
{
        int n = 0;
/*
** The loop will execute once for each bit of x set, this is in average
** twice as fast as the shift/test method.
*/
        if (x) do
              n++;
        while (0 != (x = x&(x-1))) ;
        return(n);
}
int bitcount(uint32_t i)
{
      i = ((i & 0xAAAAAAAAL) >>  1) + (i & 0x55555555L);
      i = ((i & 0xCCCCCCCCL) >>  2) + (i & 0x33333333L);
      i = ((i & 0xF0F0F0F0L) >>  4) + (i & 0x0F0F0F0FL);
      i = ((i & 0xFF00FF00L) >>  8) + (i & 0x00FF00FFL);
      i = ((i & 0xFFFF0000L) >> 16) + (i & 0x0000FFFFL);
      return (int)i;
}
int ntbl_bitcount(uint32_t x)
{
      return
            bits[ (int) (x & 0x0000000FUL)       ] +
            bits[ (int)((x & 0x000000F0UL) >> 4) ] +
            bits[ (int)((x & 0x00000F00UL) >> 8) ] +
            bits[ (int)((x & 0x0000F000UL) >> 12)] +
            bits[ (int)((x & 0x000F0000UL) >> 16)] +
            bits[ (int)((x & 0x00F00000UL) >> 20)] +
            bits[ (int)((x & 0x0F000000UL) >> 24)] +
            bits[ (int)((x & 0xF0000000UL) >> 28)];
}
int BW_btbl_bitcount(uint32_t x)
{
      union 
      { 
            unsigned char ch[4]; 
            long y; 
      } U; 
 
      U.y = x; 
 
      return bits[ U.ch[0] ] + bits[ U.ch[1] ] + 
             bits[ U.ch[3] ] + bits[ U.ch[2] ]; 
}
int AR_btbl_bitcount(uint32_t x)
{
      unsigned char * Ptr = (unsigned char *) &x ;
      int Accu ;

      Accu  = bits[ *Ptr++ ];
      Accu += bits[ *Ptr++ ];
      Accu += bits[ *Ptr++ ];
      Accu += bits[ *Ptr ];
      return Accu;
}
int ntbl_bitcnt(uint32_t x)
{
      int cnt = bits[(int)(x & 0x0000000FL)];

      if (0L != (x >>= 4))
            cnt += ntbl_bitcnt(x);

      return cnt;
}

static int bit_shifter(uint32_t x)
{
  int i, n;
  for (i = n = 0; x && (i < (sizeof(uint32_t) * CHAR_BIT)); ++i, x >>= 1)
    n += (int)(x & 1L);
  return n;
}
int main()
{
	chkpt_mask_init = 1; // TODO: for now, special case for init ( But do we need this at all? )
	init();
	restore_regs();
	chkpt_mask_init = 0;

	while (1) {
		// do not optimize these away!!
		unsigned volatile n_0, n_1, n_2, n_3, n_4, n_5, n_6;
#ifdef LOGIC
	// Out high
	GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_0);
	// Out low
	GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_0);
#else
		;
		PROTECT_BEGIN();
		PRINTF("start\r\n");
		PROTECT_END();
#endif
		for (unsigned ii = 0; ii < LOOP_BC; ++ii) {
			uint32_t seed;
			unsigned iter;
			unsigned func;

			n_0=0;
			n_1=0;
			n_2=0;
			n_3=0;
			n_4=0;
			n_5=0;
			n_6=0;

			for (func = 0; func < 7; func++) {
				seed = (uint32_t)SEED;
				if(func == 0){
					for(iter = 0; iter < ITER; ++iter, seed += 13){
						n_0 += bit_count(seed);
					}
				}
				else if(func == 1){
					for(iter = 0; iter < ITER; ++iter, seed += 13){
						n_1 += bitcount(seed);
					}
				}
				else if(func == 2){
					for(iter = 0; iter < ITER; ++iter, seed += 13){
						n_2 += ntbl_bitcnt(seed);
					}
				}
				else if(func == 3){
					for(iter = 0; iter < ITER; ++iter, seed += 13){
						n_3 += ntbl_bitcount(seed);
					}
				}
				else if(func == 4){
					for(iter = 0; iter < ITER; ++iter, seed += 13){
						n_4 += BW_btbl_bitcount(seed);
					}
				}
				else if(func == 5){
					for(iter = 0; iter < ITER; ++iter, seed += 13){
						n_5 += AR_btbl_bitcount(seed);
					}
				}
				else if(func == 6){
					for(iter = 0; iter < ITER; ++iter, seed += 13){
						n_6 += bit_shifter(seed);
					}
				}
			}
		}

//		if (n_0 == 502 && n_1 == 502 && n_2 == 502 &&
//			n_3 == 502 && n_4 == 502 && n_5 == 502 && n_6 == 502) {
//		}
//		else {
//			while (1);
//		}
#ifdef LOGIC
		GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_2);
		GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_2);
#else
		;
		PROTECT_BEGIN();
		PRINTF("%u\r\n", n_0);
		PROTECT_END();
		;
		PROTECT_BEGIN();
		PRINTF("%u\r\n", n_1);
		PROTECT_END();
		;
		PROTECT_BEGIN();
		PRINTF("%u\r\n", n_2);
		PROTECT_END();
		;
		PROTECT_BEGIN();
		PRINTF("%u\r\n", n_3);
		PROTECT_END();
		;
		PROTECT_BEGIN();
		PRINTF("%u\r\n", n_4);
		PROTECT_END();
		;
		PROTECT_BEGIN();
		PRINTF("%u\r\n", n_5);
		PROTECT_END();
		;
		PROTECT_BEGIN();
		PRINTF("%u\r\n", n_6);
		PROTECT_END();
#endif
	}
	return 0;
}

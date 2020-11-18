#include <msp430.h>
#include <libmsp/mem.h>
#include <libmsp/gpio.h>

#define disable_watchdog() WDTCTL = WDTPW + WDTHOLD
#define unlock_gpio() PM5CTL0 &= ~LOCKLPM5
#define reset_gpio() \
	P1OUT = 0;\
	P1DIR = 0xFF;\
	P2OUT = 0;\
	P2DIR = 0xFF;\
	P3OUT = 0;\
	P3DIR = 0xFF;\
	P4OUT = 0;\
	P4DIR = 0xFF;\
	P5OUT = 0;\
	P5DIR = 0xFF;\
	P6OUT = 0;\
	P6DIR = 0xFF;\
	P7OUT = 0;\
	P7DIR = 0xFF;\
	P8DIR = 0xFF;\
	P8OUT = 0;\
	PJOUT = 0;\
	PJDIR = 0xFFFF;

// DC0: 8MHz
#define init_clock()\
	CSCTL0_H = CSKEY_H;\
	CSCTL1 = DCOFSEL_6;\
	CSCTL2 = SELM__DCOCLK | SELS__DCOCLK | SELA__VLOCLK;\
	CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;\
	CSCTL4 = LFXTOFF | HFXTOFF;\
	CSCTL0_H = 0;

enum {RISING, FALLING};
int mode = RISING;

int main() {
	disable_watchdog();
	reset_gpio();
	unlock_gpio();
	init_clock();
	__enable_interrupt();

	// This program is simple. If 3.3V comes into P1.4, P1.5 becomes 0V
	// else, P1.5 becomes 3.3V

	// P1.5: 3.3V out for charging the cap
	P1DIR |= BIT5;
	P1OUT |= BIT5;

	// 1.4: GPIO input	
	//P1SEL &= ~BIT4;
	P1DIR &= ~BIT4;
	P1IES &= ~BIT4; // At first, detect rising edge 
	P1IFG &= ~BIT4;
	P1IE |= BIT4;
	while (1) {

	}
	return 0;
}

void __attribute__((interrupt(PORT1_VECTOR))) gpioISRHandler(void) {
	P1IFG &= ~BIT4;
	if (mode == RISING) {
		// rising edge detected
		// shut down 3.3V and wait for falling edge
		P1OUT &= ~BIT5;
		mode = FALLING;
		P1IES |= BIT4;
	} else {
		// falling edge detected
		// turn on 3.3V and wait for rising edge
		P1OUT |= BIT5;
		mode = RISING;
		P1IES &= ~BIT4;
	}

}

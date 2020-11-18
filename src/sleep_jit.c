#include <msp430.h>
//#include <stdlib.h>

//#include <libwispbase/wisp-base.h>
//#include <libmspbuiltins/builtins.h>
//#include <libmsp/mem.h>
//#include <libmsp/periph.h>

//#include "pins.h"

int main() {
	// disable watchdog
	WDTCTL = WDTPW + WDTHOLD;
	// SVS on
	PMMCTL0 = 0xA540;
	// unlock gpio
//	PM5CTL0 &= ~LOCKLPM5;
	// Configure GPIO
#if 0
	P1OUT = 0;
	P1DIR = 0x3F;

	P2OUT = 0;
	P2DIR = 0x84;

	P3OUT = 0;
	P3DIR = 0x28;

	P4OUT = 0;
	P4DIR = 0xF3;
//
	P5OUT = 0;
	P5DIR = 0xFF;
//
	P6OUT = 0;
	P6DIR = 0xFF;

	P7OUT = 0;
	P7DIR = 0xFF;

	P8DIR = 0xFF;
	P8OUT = 0;

	PJOUT = 0;
	PJDIR = 0xAF;
#endif
// Disable the GPIO power-on default high-impedance mode to activate
// previously configured port settings
	PM5CTL0 &= ~LOCKLPM5;

	// clock setup
	//CSCTL0_H = CSKEY_H;
	//CSCTL1 = DCORSEL + DCOFSEL_3;
	//CSCTL2 = SELA_0 | SELS_3 | SELM_3;
	//CSCTL3 = DIVA_0 | DIVS_0 | DIVM_0;
	CSCTL0_H = CSKEY_H;                     // Unlock CS registers
	CSCTL1 = DCOFSEL_0;                     // Set DCO to 1 MHz
	CSCTL2 = SELM__DCOCLK | SELS__DCOCLK | SELA__VLOCLK;
	CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;   // Set all dividers to 1
	CSCTL4 = LFXTOFF | HFXTOFF;
	CSCTL0_H = 0;                           // Lock CS registers

	P1DIR |= BIT0;
	while(1) {
		P1OUT |= BIT0;
		P1OUT &= ~BIT0;
		PMMCTL0 = PMMPW | PMMSWPOR;
	}	
	//RTCHOLD = 1;
	RCCTL0 = 0x5A55;
	// SVS off
	PMMCTL0 = 0xA500;

	__bis_SR_register(GIE | LPM3_bits); 

	return 0;
}

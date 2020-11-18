#include <msp430.h>

#include <libmsp/periph.h>
#include <libmsp/sleep.h>
#ifdef JIT
#include <libjit/jit.h>
#endif

#include "power.h"
#include "reconfig.h" 

#ifdef JIT
__nv volatile unsigned chkptStart = 0;
__nv volatile unsigned chkptEnd = 0;
#endif

void capybara_wait_for_supply()
{
    // wait for BOOST_OK: supply voltage stablized
    GPIO(LIBCAPYBARA_PORT_VBOOST_OK, IES) &= ~BIT(LIBCAPYBARA_PIN_VBOOST_OK);
    GPIO(LIBCAPYBARA_PORT_VBOOST_OK, IFG) &= ~BIT(LIBCAPYBARA_PIN_VBOOST_OK);
    GPIO(LIBCAPYBARA_PORT_VBOOST_OK, IE) |= BIT(LIBCAPYBARA_PIN_VBOOST_OK);

    __disable_interrupt(); // classic lock-check-sleep pattern
    while ((GPIO(LIBCAPYBARA_PORT_VBOOST_OK, IN) & BIT(LIBCAPYBARA_PIN_VBOOST_OK)) !=
                BIT(LIBCAPYBARA_PIN_VBOOST_OK)) {
        __bis_SR_register(LPM4_bits + GIE);
        __disable_interrupt();
    }
    __enable_interrupt();

    GPIO(LIBCAPYBARA_PORT_VBOOST_OK, IE) &= ~BIT(LIBCAPYBARA_PIN_VBOOST_OK);
    GPIO(LIBCAPYBARA_PORT_VBOOST_OK, IFG) &= ~BIT(LIBCAPYBARA_PIN_VBOOST_OK);
}

void capybara_wait_for_banks()
{
    // wait for VBANK_OK: cap voltage is high enough
    GPIO(LIBCAPYBARA_PORT_VBANK_OK, IES) &= ~BIT(LIBCAPYBARA_PIN_VBANK_OK);
    GPIO(LIBCAPYBARA_PORT_VBANK_OK, IFG) &= ~BIT(LIBCAPYBARA_PIN_VBANK_OK);
    GPIO(LIBCAPYBARA_PORT_VBANK_OK, IE) |= BIT(LIBCAPYBARA_PIN_VBANK_OK);

    __disable_interrupt(); // classic lock-check-sleep pattern
    while ((GPIO(LIBCAPYBARA_PORT_VBANK_OK, IN) & BIT(LIBCAPYBARA_PIN_VBANK_OK)) !=
                BIT(LIBCAPYBARA_PIN_VBANK_OK)) {
        __bis_SR_register(LPM4_bits + GIE);
        __disable_interrupt();
    }
    __enable_interrupt();

    GPIO(LIBCAPYBARA_PORT_VBANK_OK, IE) &= ~BIT(LIBCAPYBARA_PIN_VBANK_OK);
    GPIO(LIBCAPYBARA_PORT_VBANK_OK, IFG) &= ~BIT(LIBCAPYBARA_PIN_VBANK_OK);
}

int capybara_report_vbank_ok()
{   int vbank_ok_val = 0;  
    vbank_ok_val = GPIO(LIBCAPYBARA_PORT_VBANK_OK,IN) & BIT(LIBCAPYBARA_PIN_VBANK_OK); 
    return vbank_ok_val; 
}

void capybara_wait_for_vcap()
{
    // Wait for Vcap to recover
    // NOTE: this is the crudest implementation: sleep for a fixed interval
    // Alternative implemenation: use Vcap supervisor, comparator, or ADC
    msp_sleep(PERIOD_LIBCAPYBARA_VCAP_RECOVERY_TIME);
}

void capybara_shutdown()
{
    // Disable booster
    GPIO(LIBCAPYBARA_PORT_BOOST_SW, OUT) |= BIT(LIBCAPYBARA_PIN_BOOST_SW);

    // Sleep, while we wait for supply voltage to drop
    __disable_interrupt();
    while (1) {
        __bis_SR_register(LPM4_bits);
    }
}

cb_rc_t capybara_shutdown_on_deep_discharge()
{

#if defined(__MSP430FR5949__) || defined(__MSP430FR5994__)
    GPIO(LIBCAPYBARA_VBANK_COMP_PIN_PORT, SEL0) |= BIT(LIBCAPYBARA_VBANK_COMP_PIN_PIN);
    GPIO(LIBCAPYBARA_VBANK_COMP_PIN_PORT, SEL1) |= BIT(LIBCAPYBARA_VBANK_COMP_PIN_PIN);
#else // device
#error Comparator pin SEL config not implemented for device.
#endif // device

    // Configure comparator to interrupt when Vcap drops below a threshold
    COMP_VBANK(CTL3) |= COMP2_VBANK(PD, LIBCAPYBARA_VBANK_COMP_CHAN);
    COMP_VBANK(CTL0) = COMP_VBANK(IMEN) | COMP2_VBANK(IMSEL_, LIBCAPYBARA_VBANK_COMP_CHAN);
    // REF applied to resistor ladder, ladder tap applied to V+ terminal
    COMP_VBANK(CTL2) = COMP_VBANK(RS_2) | COMP2_VBANK(REFL_, LIBCAPYBARA_DEEP_DISCHARGE_REF) |
                       COMP2_VBANK(REF0_, LIBCAPYBARA_DEEP_DISCHARGE_DOWN) |
                       COMP2_VBANK(REF1_, LIBCAPYBARA_DEEP_DISCHARGE_UP);
    // Turn comparator on in ultra-low power mode
    COMP_VBANK(CTL1) |= COMP_VBANK(PWRMD_2) | COMP_VBANK(ON);

    // Let the comparator output settle before checking or setting up interrupt
    msp_sleep(PERIOD_LIBCAPYBARA_VBANK_COMP_SETTLE);

    if (COMP_VBANK(CTL1) & COMP_VBANK(OUT)) {
        // Vcap already below threshold
        return CB_ERROR_ALREADY_DEEPLY_DISCHARGED;
    }

    // Clear int flag and enable int
    COMP_VBANK(INT) &= ~(COMP_VBANK(IFG) | COMP_VBANK(IIFG));
    COMP_VBANK(INT) |= COMP_VBANK(IE);
    // If manually issuing precharge commands
    #ifdef LIBCAPYBARA_EXPLICIT_PRECHG
        // Check if a burst completed 
        if(burst_status == 2){
            // Revert to base configuration
            capybara_config_banks(base_config.banks);
        }
        // Check if a burst started, and did not complete
        //This may not be strictly necessary, but for now, leave it please
        // :) 
        else if(burst_status == 1){
            capybara_config_banks(prechg_config.banks); 
        }
        //Otherwise we stay in whatever config we had before
    #endif

    return CB_SUCCESS;
}

// Own the ISR for now, if need be can make a function, to let main own the ISR
ISR(COMP_VECTOR(LIBCAPYBARA_VBANK_COMP_TYPE))
{
	switch (__even_in_range(COMP_VBANK(IV), 0x4)) {
		case COMP_INTFLAG2(LIBCAPYBARA_VBANK_COMP_TYPE, IIFG):
			break;
		case COMP_INTFLAG2(LIBCAPYBARA_VBANK_COMP_TYPE, IFG):
			COMP_VBANK(INT) &= ~COMP_VBANK(IE);
			COMP_VBANK(CTL1) &= ~COMP_VBANK(ON);
			// If manually issuing precharge commands
#ifdef LIBCAPYBARA_EXPLICIT_PRECHG
			// Check if a burst completed 
			if(burst_status == 2){
				// Revert to base configuration
				capybara_config_banks(base_config.banks);
			}
			// Check if a burst started, and did not complete
			//This may not be strictly necessary, but for now, leave it please
			// :) 
			else if(burst_status == 1){
				capybara_config_banks(prechg_config.banks); 
			}
			//Otherwise we stay in whatever config we had before
#endif
			// KWMAENG: CHECKPOINT
			// |	lr(MSB) | <- 50(r1)
			// |		r2	  | <- 48(r1)
			// |	r15(MSB)|
			// |	r15(LSB)|
			// |	r14(MSB)|
			// |	r14(LSB)|
			// |	r13(MSB)|
			// |	r13(LSB)|
			// |	r12(MSB)|
			// |	r12(LSB)|
			// |	r11(MSB)|
			// |	r11(LSB)|
			// |	r10(MSB)|
			// |	r10(LSB)|
			// |	r9 (MSB)|
			// |	r9 (LSB)|
			// |	r8 (MSB)|
			// |	r8 (LSB)|
			// |	r7 (MSB)|
			// |	r7 (LSB)|
			// |	r6 (MSB)|
			// |	r6 (LSB)|
			// |	r5 (MSB)|
			// |	r5 (LSB)|
			// |	r4 (MSB)|
			// |	r4 (LSB)| <- r1
			// Save to 0x4400 ~0x443e
#ifdef JIT
			if (!(chkpt_mask || chkpt_mask_init)) {
				chkpt_taken = 1;
				__asm__ volatile ("MOV 50(R1), &0x4400");//r0
				__asm__ volatile ("MOV 48(R1), &0x4408");//r2
				__asm__ volatile ("MOVX.A 0(R1), &0x4410");//r4
				__asm__ volatile ("MOVX.A 4(R1), &0x4414");//r5
				__asm__ volatile ("MOVX.A 8(R1), &0x4418");
				__asm__ volatile ("MOVX.A 12(R1), &0x441c");
				__asm__ volatile ("MOVX.A 16(R1), &0x4420");
				__asm__ volatile ("MOVX.A 20(R1), &0x4424");
				__asm__ volatile ("MOVX.A 24(R1), &0x4428");
				__asm__ volatile ("MOVX.A 28(R1), &0x442c");
				__asm__ volatile ("MOVX.A 32(R1), &0x4430");
				__asm__ volatile ("MOVX.A 36(R1), &0x4434");
				__asm__ volatile ("MOVX.A 40(R1), &0x4438");
				__asm__ volatile ("MOVX.A 44(R1), &0x443c");
				__asm__ volatile ("ADD #52, R1");
				__asm__ volatile ("MOVX.A R1, &0x4404"); //r1 (- 52)
				__asm__ volatile ("SUB #52, R1");
			}
			P1OUT |= BIT3;
			P1OUT &= ~BIT3;
//			P1OUT &= ~BIT0;
#endif
			capybara_shutdown();
			break;
	}
}

#include <msp430.h>
#include <libmsp/periph.h>
#include <stdint.h>

#include "temp.h"

#define FP 10 // fixed-point fractional part
#define ADC_SCALE 4096

int temp_sample() {
  ADC12CTL0 &= ~ADC12ENC;           // Disable conversions

  ADC12CTL3 |= ADC12TCMAP;
  ADC12CTL1 = ADC12SHP;
  ADC12CTL2 = ADC12RES_2;
  ADC12MCTL0 = ADC12VRSEL_1 | ADC12EOS | LIBTEMP_ADC_CHAN;
  ADC12CTL0 |= ADC12SHT02 | ADC12SHT01 | ADC12ON; // 128 cycles sample-and-hold

  while( REFCTL0 & REFGENBUSY );

  REFCTL0 = REF(LIBTEMP_REF_BITS) | REFON;

  // Wait for REF to settle
  // TODO: to use msp_sleep, we need to take the (clock+divider) as arg
  __delay_cycles(PERIOD_LIBMSP_REF_SETTLE_TIME);
  //msp_sleep(PERIOD_LIBMSP_REF_SETTLE_TIME);

  ADC12CTL0 |= ADC12ENC;                  // Enable conversions
  ADC12CTL0 |= ADC12SC;                   // Start conversion
  ADC12CTL0 &= ~ADC12SC;                  // We only need to toggle to start conversion
  while (ADC12CTL1 & ADC12BUSY);

  int sample = ADC12MEM0;

  ADC12CTL0 &= ~ADC12ENC;           // Disable conversions
  ADC12CTL0 &= ~(ADC12ON);  // Shutdown ADC12
  REFCTL0 &= ~REFON;

  int tempC = ((int32_t)sample * LIBTEMP_ADC_CONV_FACTOR * FP / ADC_SCALE) - LIBTEMP_ADC_CONV_OFFSET * FP;

  return tempC;
}

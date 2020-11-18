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

#include "pins.h"

DSPLIB_DATA(lea_src1, 4) _q15 lea_src1[2][2] = {{_Q15(0.1), _Q15(0.2)}, {_Q15(0.3), _Q15(0.4)}};
DSPLIB_DATA(lea_src2, 4) _q15 lea_src2[2][2] = {{_Q15(0.1), _Q15(0)}, {_Q15(0.3), _Q15(0)}};
DSPLIB_DATA(lea_dest, 4) _q15 lea_dest[2][2];
__nv _q15 expected[2][1] = {{_Q15(0.07)}, {_Q15(0.15)}};

int main()
{
	msp_status status;
	msp_matrix_mpy_q15_params mpyParams;

	WDTCTL = WDTPW + WDTHOLD;

	mpyParams.srcARows = 2;
	mpyParams.srcACols = 2;
	mpyParams.srcBRows = 2;
	mpyParams.srcBCols = 2;

	status = msp_matrix_mpy_q15(&mpyParams, *lea_src1, *lea_src2, *lea_dest);

	return 0;

}

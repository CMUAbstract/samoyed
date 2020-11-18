#include <msp430.h>

#include <libmsp/periph.h>

#include "capybara.h"

void capybara_config_pins()
{
    GPIO(LIBCAPYBARA_PORT_BOOST_SW, OUT) &= ~BIT(LIBCAPYBARA_PIN_BOOST_SW);
    GPIO(LIBCAPYBARA_PORT_BOOST_SW, DIR) |= BIT(LIBCAPYBARA_PIN_BOOST_SW);
}

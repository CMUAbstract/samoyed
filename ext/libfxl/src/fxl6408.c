#include <msp430.h>
#include <stdint.h>

#include <libio/console.h>

#include "fxl6408.h"

#define FXL_ADDR 0x43 // 100 0011

#define FXL_REG_ID        0x01
#define FXL_REG_IO_DIR    0x03
#define FXL_REG_OUT_STATE 0x05
#define FXL_REG_HIGH_Z    0x07
#define FXL_REG_PULL_DIR  0x0D

#define FXL_ID_SW_RST   0x1
#define FXL_ID_MFG_MASK 0xE0
#define FXL_ID_MFG      0xA0

static uint8_t out_state = 0x0;
static uint8_t dir_state = 0x0;
static uint8_t pull_state = 0x0;

static void set_reg(unsigned reg, unsigned val)
{
  UCB0CTLW0 |= UCTR | UCTXSTT; // transmit mode and start

  // Have to wait for addr transmission to finish, otherwise the TXIFG does not
  // behave as expected (both reg and val writes below fall through, despite
  // waits on TXIFG).
  while((UCB0CTLW0 & UCTXSTT));

  while(!(UCB0IFG & UCTXIFG));
  UCB0TXBUF = reg;

  while(!(UCB0IFG & UCTXIFG));
  UCB0TXBUF = val;

  while(!(UCB0IFG & UCTXIFG));
  UCB0CTLW0 |= UCTXSTP; // stop

  while (UCB0STATW & UCBBUSY);
}

fxl_status_t fxl_init()
{
  UCB0CTLW0 |= UCSWRST; // disable
  UCB0I2CSA = FXL_ADDR;
  UCB0CTLW0 &= ~UCSWRST; // enable

  while (UCB0STATW & UCBBUSY);

  UCB0CTLW0 |= UCTR | UCTXSTT; // transmit mode and start
  while(!(UCB0IFG & UCTXIFG));
  UCB0TXBUF = FXL_REG_ID;
  while(!(UCB0IFG & UCTXIFG));

  UCB0CTLW0 &= ~UCTR; // receive mode
  UCB0CTLW0 |= UCTXSTT; // repeated start

  // wait for addr transmission to finish, data transfer to start
  while(UCB0CTLW0 & UCTXSTT);
  UCB0CTLW0 |= UCTXSTP; // stop

  while(!(UCB0IFG & UCRXIFG));
  uint8_t id = UCB0RXBUF;

  while (UCB0STATW & UCBBUSY);

  if ((id & FXL_ID_MFG_MASK) != FXL_ID_MFG) {
    LOG("FXL: invalid MFG id: 0x%02x (expected 0x%02x)\r\n", id & FXL_ID_MFG_MASK, FXL_ID_MFG);
    return FXL_ERR_BAD_ID;
  }

  LOG2("FXL: id 0x%02x\r\n", id);

  set_reg(FXL_REG_ID, id | FXL_ID_SW_RST); // reset to synch with local state
  out_state = 0x00;

  set_reg(FXL_REG_HIGH_Z, 0x00); // output from Output State reg, not High-Z

  return FXL_SUCCESS;
}

fxl_status_t fxl_out(uint8_t bit)
{
    dir_state |= bit;
    set_reg(FXL_REG_IO_DIR, dir_state);
    return FXL_SUCCESS;
}

fxl_status_t fxl_in(uint8_t bit)
{
    dir_state &= ~bit;
    set_reg(FXL_REG_IO_DIR, dir_state);
    return FXL_SUCCESS;
}

fxl_status_t fxl_pull_up(uint8_t bit)
{
    pull_state |= bit;
    set_reg(FXL_REG_PULL_DIR, pull_state);
    return FXL_SUCCESS;
}

fxl_status_t fxl_pull_down(uint8_t bit)
{
    pull_state &= ~bit;
    set_reg(FXL_REG_PULL_DIR, pull_state);
    return FXL_SUCCESS;
}

fxl_status_t fxl_set(uint8_t bit)
{
    out_state |= bit;
    set_reg(FXL_REG_OUT_STATE, out_state);
    return FXL_SUCCESS;
}

fxl_status_t fxl_clear(uint8_t bit)
{
    out_state &= ~bit;
    set_reg(FXL_REG_OUT_STATE, out_state);
    return FXL_SUCCESS;
}

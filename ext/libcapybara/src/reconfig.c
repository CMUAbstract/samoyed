#include <msp430.h>

#ifdef LIBCAPYBARA_VARTH_ENABLED
#include <libmcppot/mcp4xxx.h>
#endif // LIBCAPYBARA_VARTH_ENABLED

#include <libmsp/periph.h>
#include <libmsp/mem.h>

#include "reconfig.h"

/* Working config and precharged config */ 
#ifdef LIBCAPYBARA_VARTH_ENABLED
__nv capybara_cfg_t base_config = {0} ; 
#else
__nv capybara_cfg_t base_config = {.banks = 0x1}; 
#endif 

__nv capybara_cfg_t prechg_config = {0}; 

/* Precharge and Burst status globals */ 
__nv prechg_status_t prechg_status = 0;  
__nv burst_status_t burst_status = 0; 
volatile prechg_status_t v_prechg_status;
volatile burst_status_t v_burst_status; 

/* Leaving these simple for now... I can't see them ever getting too complicated, but who
 * knows? TODO turn these into macros so we don't have to pay for a function call every
 * single time they're accessed... 
 */

prechg_status_t get_prechg_status(void){
    return (prechg_status); 
}

int set_prechg_status(prechg_status_t in){
    prechg_status = in; 
    return 0; 
}

burst_status_t get_burst_status(void){
    return (burst_status); 
}

int set_burst_status(burst_status_t in){
    burst_status = in; 
    return 0; 
}

int set_base_banks(capybara_bankmask_t in){
    base_config.banks = in;
    return 0; 
}

int set_prechg_banks(capybara_bankmask_t in){
    prechg_config.banks = in;
    return 0; 
}

//  Command from userspace code to issue a precharge, takes bank config as
//  an argument (TODO: figure out if this is the best place for this function to
//  live... I suspect it doesn't belong back here given how closely its usage is
//  tied to libchain)
int issue_precharge(capybara_bankmask_t cfg){
    prechg_config.banks = cfg; 
    prechg_status = 1; 
    return 0; 
}
#ifdef LIBCAPYBARA_VARTH_ENABLED
#define X(a, b, c) {.banks = b, .vth = c},
#else
#define X(a, b, c) {.banks = b},
#endif

capybara_cfg_t pwr_levels[] = {
    PWR_LEVEL_TABLE
    #undef X
};

// Cycles for the latch cap to charge/discharge
#define SWITCH_TIME_CYCLES 6400 // charges to ~2.2v

#if defined(LIBCAPYBARA_SWITCH_CONTROL__ONE_PIN)

#define BANK_PORT_INNER(i) LIBCAPYBARA_BANK_PORT_ ## i ## _PORT
#define BANK_PORT(i) BANK_PORT_INNER(i)

#define BANK_PIN_INNER(i) LIBCAPYBARA_BANK_PORT_ ## i ## _PIN
#define BANK_PIN(i) BANK_PIN_INNER(i)

#elif defined(LIBCAPYBARA_SWITCH_CONTROL__TWO_PIN)

#define BANK_PORT_INNER(i, op) LIBCAPYBARA_BANK_PORT_ ## i ## _ ## op ## _PORT
#define BANK_PORT(i, op) BANK_PORT_INNER(i, op)

#define BANK_PIN_INNER(i, op) LIBCAPYBARA_BANK_PORT_ ## i ## _ ## op ## _PIN
#define BANK_PIN(i, op) BANK_PIN_INNER(i, op)

#endif // LIBCAPYBARA_SWITCH_CONTROL

#if LIBCAPYBARA_NUM_BANKS != 4 // safety-check, if you change this change the following code!
#error Fixed number of banks in implementation differs from the configuration.
#endif // LIBCAPYBARA_NUM_BANKS

// If bank switch lines on the same port and the switch interface is one-pin,
// we can config all banks at once.
//
// NOTE: We support the all-at-once config only if pins are consecutive. We
// could support simultaneous config for non-consecutive pins too, by remapping
// bit positions in the API getters/setters (which is better than remapping
// them on every config; also, we don't want to expose hw pin to the API
// consumer, for the consumer, bit 0 in the bitmask always refers to bank0).
// But, for now we just fall back to non-simulatenous impl in the
// non-consecutive case.
#if defined(LIBCAPYBARA_SWITCH_CONTROL__ONE_PIN) && \
    (BANK_PORT(0) == BANK_PORT(1) == BANK_PORT(2) == BANK_PORT(3)) && \
    (BANK_PIN(1) - BANK_PIN(0) == 1) && \
    (BANK_PIN(2) - BANK_PIN(1) == 1) && \
    (BANK_PIN(3) - BANK_PIN(2) == 1)

#define BANK_PINS \
    (BIT(BANK_PIN(0)) | \
     BIT(BANK_PIN(1)) | \
     BIT(BANK_PIN(2)) | \
     BIT(BANK_PIN(3)))

#define CONNECT_LATCHES(banks) \
        GPIO(BANK_PORT(0), DIR) |= BANK_PINS

#define DISCONNECT_LATCHES(banks) \
        GPIO(BANK_PORT(0), DIR) &= ~BANK_PINS

// We don't want to touch the rest of the pins on this port, so can't do it in
// one assignment.
#define CONFIG_BANKS_COMMON(banks) do { \
        GPIO(BANK_PORT(0), OUT) &= ~BANK_PINS; \
        GPIO(BANK_PORT(0), OUT) |= ((banks) << BANK_PIN(0)); \
    } while (0);

#if defined(LIBCAPYBARA_SWITCH_DESIGN__NC)
#define CONFIG_BANKS(banks) CONFIG_BANKS_COMMON(~(banks))
#elif defined(LIBCAPYBARA_SWITCH_DESIGN__NO)
#define CONFIG_BANKS(banks) CONFIG_BANKS_COMMON(banks)
#else // LIBCAPYBARA_SWITCH_DESIGN
#error Invalid value of config option: LIBCAPYBARA_SWITCH_DESIGN
#endif // LIBCAPYBARA_SWITCH_DESIGN

#else // !"same port" (configure pins for each bank in a separate instruction)


#if defined(LIBCAPYBARA_SWITCH_CONTROL__ONE_PIN)

#define CONNECT_LATCH(i, op) \
        GPIO(BANK_PORT(i), DIR) |= BIT(BANK_PIN(i))

#define DISCONNECT_LATCH(i, op) \
        GPIO(BANK_PORT(i), DIR) &= ~BIT(BANK_PIN(i))

#elif defined(LIBCAPYBARA_SWITCH_CONTROL__TWO_PIN)

#define CONNECT_LATCH(i, op) \
        GPIO(BANK_PORT(i, op), DIR) |= BIT(BANK_PIN(i, op))

#define DISCONNECT_LATCH(i, op) \
        GPIO(BANK_PORT(i, op), DIR) &= ~BIT(BANK_PIN(i, op))

#endif // LIBCAPYBARA_SWITCH_CONTROL


#if defined(LIBCAPYBARA_SWITCH_DESIGN__NC)

#if defined(LIBCAPYBARA_SWITCH_CONTROL__TWO_PIN)

#define BANK_CONNECT(i) \
    GPIO(BANK_PORT(i, CLOSE), OUT) |= BIT(BANK_PIN(i, CLOSE))

#define BANK_DISCONNECT(i) \
        GPIO(BANK_PORT(i, OPEN), OUT) |= BIT(BANK_PIN(i, OPEN))

#elif defined(LIBCAPYBARA_SWITCH_CONTROL__ONE_PIN)

#define BANK_CONNECT(i) \
        GPIO(BANK_PORT(i), OUT) &= ~BIT(BANK_PIN(i))

#define BANK_DISCONNECT(i) \
        GPIO(BANK_PORT(i), OUT) |= BIT(BANK_PIN(i))

#else // LIBCAPYBARA_SWITCH_CONTROL
#error Invalid value of config option: LIBCAPYBARA_SWITCH_CONTROL
#endif // LIBCAPYBARA_SWITCH_CONTROL

#elif defined(LIBCAPYBARA_SWITCH_DESIGN__NO)

#if defined(LIBCAPYBARA_SWITCH_CONTROL__TWO_PIN)
#error Not implemented: switch design NO, switch control TWO PIN
#elif defined(LIBCAPYBARA_SWITCH_CONTROL__ONE_PIN)

#define BANK_CONNECT(i) \
        GPIO(BANK_PORT(i), OUT) |= BIT(BANK_PIN(i))

#define BANK_DISCONNECT(i) \
        GPIO(BANK_PORT(i), OUT) &= ~BIT(BANK_PIN(i))

#endif // LIBCAPYBARA_SWITCH_CONTROL

#else // LIBCAPYBARA_SWITCH_DESIGN
#error Invalid value of config option: LIBCAPYBARA_SWITCH_DESIGN
#endif // LIBCAPYBARA_SWITCH_DESIGN

// The following are not loops because we don't want runtime overhead

#define CONFIG_BANK(banks, i) \
    if (banks & (1 << i)) { BANK_CONNECT(i); } else { BANK_DISCONNECT(i); }
#define DO_CONNECT_LATCH(banks, i) \
    if (banks & (1 << i)) { CONNECT_LATCH(i, CLOSE); } else { CONNECT_LATCH(i, OPEN); }
#define DO_DISCONNECT_LATCH(banks, i) \
    if (banks & (1 << i)) { DISCONNECT_LATCH(i, CLOSE); } else { DISCONNECT_LATCH(i, OPEN); }

#define CONFIG_BANKS(banks) \
    CONFIG_BANK(banks, 0); \
    CONFIG_BANK(banks, 1); \
    CONFIG_BANK(banks, 2); \
    CONFIG_BANK(banks, 3); \

#define CONNECT_LATCHES(banks) \
    DO_CONNECT_LATCH(banks, 0); \
    DO_CONNECT_LATCH(banks, 1); \
    DO_CONNECT_LATCH(banks, 2); \
    DO_CONNECT_LATCH(banks, 3); \

#define DISCONNECT_LATCHES(banks) \
    DO_DISCONNECT_LATCH(banks, 0); \
    DO_DISCONNECT_LATCH(banks, 1); \
    DO_DISCONNECT_LATCH(banks, 2); \
    DO_DISCONNECT_LATCH(banks, 3); \

#endif // !"same port"

int capybara_config_banks(capybara_bankmask_t banks)
{
    // Overlap the delay for all banks (even when pins are not on same port)

    CONFIG_BANKS(banks);
    CONNECT_LATCHES(banks);
    __delay_cycles(SWITCH_TIME_CYCLES);
    DISCONNECT_LATCHES(banks);

    return 0;
}

#ifdef LIBCAPYBARA_VARTH_ENABLED
int capybara_config_threshold(uint16_t wiper)
{
    uint16_t curr_wiper = pot_get_nv_wiper();
    if (curr_wiper != wiper) {
        pot_set_nv_wiper(wiper);
        pot_set_v_wiper(wiper); // not clear if redundant, so just in case
    } else {
        pot_set_v_wiper(wiper); // just in case
    }
    return 0;
}
#endif // LIBCAPYBARA_VARTH_ENABLED

int capybara_config(capybara_cfg_t cfg)
{
    int rc;

    rc = capybara_config_banks(cfg.banks);
    if (rc) return rc;

#ifdef LIBCAPYBARA_VARTH_ENABLED
    rc = capybara_config_threshold(cfg.vth);
    if (rc) return rc;
#endif // LIBCAPYBARA_VARTH_ENABLED

    return 0;
}

int capybara_config_max()
{
#ifdef LIBCAPYBARA_VARTH_ENABLED
    capybara_cfg_t cfg = { ~0, POT_RESOLUTION };
#else
    capybara_cfg_t cfg = { ~0 };
#endif // LIBCAPYBARA_VARTH_ENABLED
    return capybara_config(cfg);
}

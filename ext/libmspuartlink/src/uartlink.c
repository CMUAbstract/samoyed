#include <msp430.h>

#include <libio/console.h>
#include <libmsp/periph.h>

#include "uartlink.h"

typedef enum {
    DECODER_STATE_HEADER  = 0x0,
    DECODER_STATE_PAYLOAD,
} decoder_state_t;

#define RX_FIFO_SIZE 32
#define RX_FIFO_SIZE_MASK 0x1f

static uint8_t rx_fifo[RX_FIFO_SIZE];
static unsigned rx_fifo_head = 0;
static unsigned rx_fifo_tail = 0;

static uint8_t *tx_data;
static unsigned tx_len;

static decoder_state_t decoder_state = DECODER_STATE_HEADER;
static unsigned rx_payload_len = 0;
static ul_header_t rx_header;

static void uartlink_configure()
{
    UART(LIBMSPUARTLINK_UART_IDX, CTL1) |= UCSWRST; // put state machine in reset
    UART(LIBMSPUARTLINK_UART_IDX, CTL1) |= CONCAT(UCSSEL__, LIBMSPUARTLINK_CLOCK);

    UART_SET_BR(LIBMSPUARTLINK_UART_IDX, LIBMSPUARTLINK_BAUDRATE_BR);
    UART_MCTL(LIBMSPUARTLINK_UART_IDX) = UCBRF_0 | UART_BRS(LIBMSPUARTLINK_BAUDRATE_BRS);

    UART(LIBMSPUARTLINK_UART_IDX, CTL1) &= ~UCSWRST; // turn on
}

#ifdef LIBMSPUARTLINK_PIN_RX_PORT
void uartlink_open_rx()
{
    UART_SET_SEL(LIBMSPUARTLINK_PIN_RX_PORT, BIT(LIBMSPUARTLINK_PIN_RX_PIN));
    uartlink_configure();
    UART(LIBMSPUARTLINK_UART_IDX, IE) |= UCRXIE;
}
#endif // LIBMSPUARTLINK_PIN_RX_PORT

#ifdef LIBMSPUARTLINK_PIN_TX_PORT
void uartlink_open_tx()
{
    UART_SET_SEL(LIBMSPUARTLINK_PIN_TX_PORT, BIT(LIBMSPUARTLINK_PIN_TX_PIN));
    uartlink_configure();
}
#endif // LIBMSPUARTLINK_PIN_TX_PORT

#if defined(LIBMSPUARTLINK_PIN_TX_PORT) && defined(LIBMSPUARTLINK_PIN_RX_PORT)
// bidirectional (not implemenented)
void uartlink_open()
{
    UART_SET_SEL(LIBMSPUARTLINK_PIN_RX_PORT, BIT(LIBMSPUARTLINK_PIN_RX_PIN) | BIT(LIBMSPUARTLINK_PIN_TX_PIN));
    uartlink_configure();
    UART(LIBMSPUARTLINK_UART_IDX, IE) |= UCRXIE;
}
#endif // LIBMSPUARTLINK_PIN_{TX && RX}_PORT

void uartlink_close()
{
    UART(LIBMSPUARTLINK_UART_IDX, CTL1) |= UCSWRST;
}

static inline void uartlink_disable_interrupt()
{
    UART(LIBMSPUARTLINK_UART_IDX, IE) &= ~UCRXIE;
}

static inline void uartlink_enable_interrupt()
{
    UART(LIBMSPUARTLINK_UART_IDX, IE) |= UCRXIE;
}

#ifdef LIBMSPUARTLINK_PIN_TX_PORT
static inline void send_byte(uint8_t *buf, unsigned len)
{
}

void uartlink_send(uint8_t *payload, unsigned len)
{
    // Payload checksum
    CRCINIRES = 0xFFFF; // initialize checksumer
    for (int i = 0; i < len; ++i) {
        CRCDI_L = *(payload + i);
    }
    uint8_t pay_chksum = CRCINIRES & UARTLINK_PAYLOAD_CHKSUM_MASK;

    ul_header_ut header = { .typed = { .size = len,
                                       .pay_chksum = pay_chksum,
                                       .hdr_chksum = 0 /* to be filled in shortly */ } };

    CRCINIRES = 0xFFFF; // initialize checksumer
    CRCDI_L = header.raw;
    header.typed.hdr_chksum = CRCINIRES & UARTLINK_HDR_CHKSUM_MASK;

    // Setup pointers for the ISR
    tx_data = payload;
    tx_len = len;
    UART(LIBMSPUARTLINK_UART_IDX, IE) |= UCTXIE;
    UART(LIBMSPUARTLINK_UART_IDX, TXBUF) = header.raw; // first byte, clears IFG

    // Sleep, while ISR TXes the remaining bytes
    //
    // We have to disable TX int from ISR, otherwise, will enter infinite ISR loop.
    // So, we might as well use it as the sleep flag.
    __disable_interrupt(); // classic lock-check-(sleep+unlock)-lock pattern
    while (UART(LIBMSPUARTLINK_UART_IDX, IE) & UCTXIE) {
        __bis_SR_register(LPM0_bits + GIE); // will wakeup after ISR TXes last byte
        __disable_interrupt();
    }
    __enable_interrupt();

    // TXIE is set before the last byte has finished transmitting
    while (UART(LIBMSPUARTLINK_UART_IDX, STATW) & UCBUSY);
}
#endif // LIBMSPUARTLINK_PIN_TX_PORT

// Should be called whenever MCU wakes up, from the context of a main loop
// precondition: payload points to a buffer of at least UARTLINK_MAX_PAYLOAD_SIZE
unsigned uartlink_receive(uint8_t *payload)
{
    unsigned rx_pkt_len = 0;

    uartlink_disable_interrupt();
    // keep processing fifo content until empty, or until got pkt
    while (rx_fifo_head != rx_fifo_tail && !rx_pkt_len) {
        uint8_t rx_byte = rx_fifo[rx_fifo_head++];
        rx_fifo_head &= RX_FIFO_SIZE_MASK; // wrap around
        uartlink_enable_interrupt();

        LOG("uartlink: rcved: %02x\r\n", rx_byte);

        switch (decoder_state) {
            case DECODER_STATE_HEADER: {

                ul_header_ut header = { .raw = rx_byte };

                ul_header_ut header_unchksumed = header;
                header_unchksumed.typed.hdr_chksum = 0;
                CRCINIRES = 0xFFFF; // initialize checksum'er for header
                CRCDI_L = header_unchksumed.raw;

                unsigned hdr_chksum_local = CRCINIRES & UARTLINK_HDR_CHKSUM_MASK;
                if (hdr_chksum_local == header.typed.hdr_chksum) {
                    if (header.typed.size > 0) {
                        LOG("hdr 0x%02x: size %u | chksum: hdr 0x%02x pay 0x%02x\r\n",
                            header.raw, header.typed.size,
                            header.typed.hdr_chksum, header.typed.pay_chksum);

                        rx_header = header.typed;
                        rx_payload_len = 0; // init packet
                        CRCINIRES = 0xFFFF; // initialize checksum'er for payload
                        decoder_state = DECODER_STATE_PAYLOAD;
                    } else {
                        LOG("uartlink: finished receiving pkt: len %u\r\n", rx_payload_len);
                        rx_pkt_len = 0;
                    }
                } else {
                    LOG("uartlink: hdr chksum mismatch (0x%02x != 0x%02x)\r\n",
                        hdr_chksum_local, header.typed.hdr_chksum);
                }
                break;
            }
            case DECODER_STATE_PAYLOAD:
                // assert: pkt.header.size < UARTLINK_MAX_PAYLOAD_SIZE
                payload[rx_payload_len++] = rx_byte;
                CRCDI_L = rx_byte;
                if (rx_payload_len == rx_header.size) {
                    // check payload checksum
                    uint8_t rx_payload_chksum = CRCINIRES & UARTLINK_PAYLOAD_CHKSUM_MASK;
                    if (rx_payload_chksum != rx_header.pay_chksum) {
                        LOG("uartlink: payload chksum mismatch (0x%02x != 0x%02x)\r\n",
                            rx_payload_chksum, rx_header.pay_chksum);
                        // reset decoder
                        rx_payload_len = 0;
                        decoder_state = DECODER_STATE_HEADER;
                    } else {
                        LOG("uartlink: finished receiving pkt: len %u\r\n", rx_payload_len);
                        rx_pkt_len = rx_payload_len;

                        // reset decoder
                        rx_payload_len = 0;
                        decoder_state = DECODER_STATE_HEADER;
                    }
                } else if (rx_payload_len == UARTLINK_MAX_PAYLOAD_SIZE) {
                    LOG("uartlink: payload too long\r\n");
                    // reset decoder
                    rx_payload_len = 0;
                    decoder_state = DECODER_STATE_HEADER;
                }
                break;
        }

        uartlink_disable_interrupt(); // classic: check-and-...-sleep pattern
    }
    uartlink_enable_interrupt();
    return rx_pkt_len;
}

__attribute__ ((interrupt(UART_VECTOR(LIBMSPUARTLINK_UART_IDX))))
void UART_ISR(LIBMSPUARTLINK_UART_IDX) (void)
{
    switch(__even_in_range(UART(LIBMSPUARTLINK_UART_IDX, IV),0x08)) {
        case UART_INTFLAG(TXIFG):
            if (tx_len--) {
                UART(LIBMSPUARTLINK_UART_IDX, TXBUF) = *tx_data++;
            } else { // last byte got done
                UART(LIBMSPUARTLINK_UART_IDX, IE) &= ~UCTXIE;
                __bic_SR_register_on_exit(LPM4_bits); // wakeup
            }
            break; // nothing to do, main thread is sleeping, so just wakeup
        case UART_INTFLAG(RXIFG):
        {
            P3OUT |= BIT2;

            rx_fifo[rx_fifo_tail] = UART(LIBMSPUARTLINK_UART_IDX, RXBUF);
            rx_fifo_tail = (rx_fifo_tail + 1) & RX_FIFO_SIZE_MASK; // wrap-around (assumes size is power of 2)

            if (rx_fifo_tail == rx_fifo_head) {
                // overflow, throw away the received byte, by rolling back
                // NOTE: tail == head happens both when full and empty, so can't use that as overflow check
                rx_fifo_tail = (rx_fifo_tail - 1) & RX_FIFO_SIZE_MASK; // wrap-around (assumes size is power of 2)
            }

            P3OUT &= ~BIT2;
            __bic_SR_register_on_exit(LPM4_bits); // wakeup
            break;
        }
        default:
            break;
    }
}

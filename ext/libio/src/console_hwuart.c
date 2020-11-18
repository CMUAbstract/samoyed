#include <stdint.h>
#include <stdlib.h>

#include <libmsp/uart.h>

void console_init()
{
    msp_uart_open();
}

int io_putchar(int c)
{
    uint8_t ch = c;
    msp_uart_send_sync(&ch, 1);
    return c;
}

int io_puts_no_newline(const char *ptr)
{
    unsigned len = 0;
    const char *p = ptr;

    while (*p++ != '\0')
        len++;

    msp_uart_send_sync((uint8_t *)ptr, len);
    return len;
}

int io_puts(const char *ptr)
{
    unsigned len;

    len = io_puts_no_newline(ptr);

    // Semantics of puts are annoying...
    io_putchar('\n');

    return len;
}

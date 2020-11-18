#include <stdint.h>

#include <libmspsoftuart/uart.h>

void console_init()
{
    mspsoftuart_init();
}

#ifdef LIBIO_IN
int io_getchar(void)
{
    return mspsoftuart_receive_byte_sync();
}
#endif // LIBIO_IN

int io_putchar(int ch)
{
    mspsoftuart_send_byte_sync(ch);
    return ch;
}

int io_puts(const char *str)
{
    while(*str != 0) io_putchar(*str++);
    io_putchar('\n'); // semantics of puts say it appends a newline
    return 0;
}

int io_puts_no_newline(const char *str)
{
    while(*str != 0) io_putchar(*str++);
    return 0;
}

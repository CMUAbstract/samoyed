#include <msp430.h>

#include <libedb/stdio.h>

int io_putchar(int c)
{
    edb_stdio_write_byte(c);
    return c;
}

int io_puts_no_newline(const char *ptr)
{
    return edb_stdio_write(ptr, /* newline */ false);
}

int io_puts(const char *ptr)
{
    return edb_stdio_write(ptr, /* newline */ true);
}

#ifndef LIBIO_CONSOLE_HWUART_H
#define LIBIO_CONSOLE_HWUART_H

#include <libmsp/uart.h>

#define INIT_CONSOLE_BACKEND() msp_uart_open()

// All special printfs fall back to the regular printf
#define BLOCK_PRINTF_BEGIN()
#define BLOCK_PRINTF(...) printf(__VA_ARGS__)
#define BLOCK_PRINTF_END()

#define EIF_PRINTF(...)  printf(__VA_ARGS__)
#define BARE_PRINTF(...) printf(__VA_ARGS__)

// Default printf also falls back to regular printf
#define PRINTF(...) printf(__VA_ARGS__)

#endif // LIBIO_CONSOLE_HWUART_H

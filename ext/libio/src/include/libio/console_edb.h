#ifndef LIBIO_CONSOLE_EDB_H
#define LIBIO_CONSOLE_EDB_H

#include <libedb/edb.h>
#include <libedb/printf.h>

#define INIT_CONSOLE_BACKEND()

// The multi-statement printf, is...
#define BLOCK_PRINTF_BEGIN() ENERGY_GUARD_BEGIN()
#define BLOCK_PRINTF(...) BARE_PRINTF(__VA_ARGS__)
#define BLOCK_PRINTF_END() ENERGY_GUARD_END()

#define PRINTF(...) EIF_PRINTF(__VA_ARGS__)

#endif // LIBIO_CONSOLE_EDB_H

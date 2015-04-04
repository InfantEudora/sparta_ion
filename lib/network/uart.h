#ifndef _UART_
#define _UART_

#include "libdebug.h"
#define uart_printf(a,l,...) printfdebug_uart(a,l,__VA_ARGS__)

#ifdef AVR
#include "uartavr.h"
#else
#include "uarttty.h"
#endif


#endif

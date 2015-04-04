//For AVR
#ifndef _UART_AVR_
#define _UART_AVR_

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "fifo.h"

typedef struct uart_s uart_s;

struct uart_s{
	fifo_t 	*rxfifo;
	fifo_t 	*txfifo;
	//In half duplex mode:
	bool 	transmitting;	//Flag if it's currently transmitting.
	void 	(*write_start)(void);
	void 	(*write_stop)(void);
};


//You'll need to implement two functions if you're using a 485 transceiver.
//write_start should enable it.
//write_stop should disable it.

void 	uart_init(uart_s* uart);

#endif

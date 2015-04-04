/*
*
*
*
*/
#ifndef __UART__
#define __UART__

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include "../../lib/network/uart.h"
#include "../../lib/network/fifo.h"

extern uart_s uarts[1];

//Pointers for naming.
#define uartbus		((uart_s*)(&uarts[0]))

//Inits all uarts.
//void uarthw_init(void);
#define uarthw_init()	uart1_init()

void uart1_init(void);
void uart1_writestart(void);
void uart1_writestop(void);

#define UART_BUFFER_SIZE		128

//TX PIN number.
#define TXPIN_1				PIN3_bm

extern fifo_t rxfifo1;
extern fifo_t txfifo1;

#endif

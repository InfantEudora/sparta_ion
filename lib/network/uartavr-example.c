#ifndef _UART_H_
#define _UART_H_

#include "uartavr.h"


ISR(USARTC1_RXC_vect){
}

ISR(USARTC1_TXC_vect){
}


ISR(USARTD0_RXC_vect){	
	fifo_putc(&rx_fifo3,USARTD0.DATA);
}

ISR(USARTD0_TXC_vect){
	uint8_t data;
	if (fifo_get(&tx_fifo3,&data)){
		USARTD0.DATA = data;
	}
}

ISR(USARTE0_RXC_vect){	
	fifo_putc(&rx_fifo0,USARTE0.DATA);
}


ISR(USARTE0_TXC_vect){
	uint8_t data;
	if (fifo_get(&tx_fifo0,&data)){
		USARTE0.DATA = data;
	}else{
		uart0_writestop();
	}
}

// Start transmissions
void uart0_writestart(void){
	if (!(uart0.transmitting)){
		uart0.transmitting  = true;		
		uint8_t data;
		if (fifo_get(&txfifo0,&data)){
			EnableTxE0(); 
			USARTE0.DATA = data;
		}else{
			uart0_writestop();
		}
	}
}

void uart1_writestart(void){
	if (!(uart1.transmitting)){
		uart1.transmitting  = true;		
		EnableTxC0(); //RS485
		uint8_t data;
		if (fifo_get(&tx_fifo1,&data)){
			USARTC0.DATA = data;
		}else{
			uart1_writestop();
		}
	}	
}

void uart2_writestart(void){
	if (!uart2_sending){
		uart2_sending  = true;
		uint8_t data;
		if (fifo_get(&txfifo2,&data)){
			EnableTxC1();
			USARTC1.DATA = data;
		}else{
			uart2_writestop();
		}
	}
}

void uart0_writestop(void){	
	DisableTxE0();
	return;
}

void uart1_writestop(void){
	uart1_sending = false;	
	DisableTxC0();
}

void uart2_writestop(void){	
	uart2_sending = false;
	DisableTxC1();
}


#endif
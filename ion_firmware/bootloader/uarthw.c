#include "app.h"
#include "uarthw.h"

uart_s uarts[1];

//UART1 is on PORTC0
fifo_t rxfifo1;
fifo_t txfifo1;

volatile uint8_t rx_buffer1[UART_BUFFER_SIZE] = {0};	
volatile uint8_t tx_buffer1[UART_BUFFER_SIZE] = {0};

void uart1_init(void){
	USARTE0.CTRLA = 0;
	
	// Initialize FIFOs
	fifo_init(&rxfifo1, rx_buffer1, UART_BUFFER_SIZE);
	fifo_init(&txfifo1, tx_buffer1, UART_BUFFER_SIZE);

	USARTE0.BAUDCTRLB = 0;
	USARTE0.BAUDCTRLA = 103;
	
	USARTE0.CTRLC = USART_CHSIZE0_bm | USART_CHSIZE1_bm;		//Character size 8-bits.
	USARTE0.CTRLB = USART_RXEN_bm | USART_TXEN_bm ;				//Enable Receive/transmit.
	USARTE0.CTRLA = USART_RXCINTLVL_LO_gc | USART_TXCINTLVL_LO_gc;
	
	PORTE.DIRSET = TXPIN_1;
	PORTE.OUTSET = TXPIN_1;
	uart1_writestop();
}

//Receive interrupt for UART1
ISR(USARTE0_RXC_vect){
	fifo_putc(&rxfifo1,USARTE0.DATA);
}

//Transmit for UART1
ISR(USARTE0_TXC_vect){
	uint8_t data;
	if (fifo_get(&txfifo1,&data)){
		USARTE0.DATA = data;
	}else{
		uart1_writestop();
	}
}

void uart1_writestart(void){
	if (!(uartbus->transmitting)){
		//enable TX?
		//PORTC.OUTSET = TXEN_1;
		uartbus->transmitting = true;
		uint8_t data;
		if (fifo_get(&txfifo1,&data)){
			USARTE0.DATA = data;
		}else{
			uart1_writestop();
		}
	}
}

void uart1_writestop(void){
	uartbus->transmitting = false;
	//PORTC.OUTCLR = TXEN_1;
}
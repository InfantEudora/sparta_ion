/*
	Created: 6/23/2014 5:07:46 PM - Copyright (c) 
	Author: D. Prins			
		Infant - infant.tweakblogs.net
		mail: prinsje2004 at gmail

	File:		uart.c
	Includes:	uart.h
				bowbus.h
				
		The BOW Bus works at 9600 BAUD.
		The UART rate is pretty unstable on internal RC clock, so there is a uart_rate_find
		which adjusts the frequency slightly to compensate for clock/temperature variations.
		The RX/TX ISR both provide character escaping, and syncing.

	This code is released under GNU GPL v3:

    	This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
	
	    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

	    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <avr/interrupt.h>
#include <stdint.h>
#include "uart.h"
#include "../../lib_ion/bowbus.h"

//UART rate settings 
uint8_t bscale = 0;
uint8_t bsel = 200;

/*
	Increase the UART rate by a tiny bit.
*/
void uart_rate_find(void){
	if (bsel < BSEL_MAX){
		bsel++;
	}else{
		bsel = BSEL_MIN;
	}
	//Apply new rate.
	USARTE0.BAUDCTRLA = bsel;
}

/*
	The order in which you do things here is quite important:
		First Clear CTLRA.
		Set the Rate.
		The to CTRLC then B, then A.
*/
void uart0_init(){
	USARTE0.CTRLA = 0;

	//Set initial baud rate.
	USARTE0.BAUDCTRLB = bscale;
	USARTE0.BAUDCTRLA = bsel;	
	
	//Settings.
	USARTE0.CTRLC = USART_CHSIZE0_bm | USART_CHSIZE1_bm;			//Character size 8-bits.
	USARTE0.CTRLB = USART_RXEN_bm | USART_TXEN_bm ;					//Enable Receive/transmit.
	USARTE0.CTRLA = USART_RXCINTLVL_HI_gc | USART_TXCINTLVL_HI_gc;	//Give RX/TX a high prio interrupt.
}

void uart_init(void){
	//Enable Outputs.
	PORTE.DIRSET = TXPIN_0;
	//Init all uarts:
	uart0_init();
}

//Reception Complete Interrupt
ISR(USARTE0_RXC_vect){		
	uint8_t data = USARTE0.DATA;	//Read data	
	bus_receive(&bus,data);			//Hand character to the bus manager.
}

//Transmission Complete Interrupt
ISR(USARTE0_TXC_vect){
	if (bus.tx_state == NET_STATE_ESCAPING){
		//We need to send a escape character.
		USARTE0.DATA = FRAME_ESCAPE;
		bus.tx_state = NET_STATE_WRITING;
	}else if (bus.tx_state == NET_STATE_WRITING){
		if(bus.tx_buff_size - bus.tx_buff_cnt){
			USARTE0.DATA = bus.tx_buff[bus.tx_buff_cnt];
			//Needs escaping?
			if (bus.tx_buff[bus.tx_buff_cnt] == FRAME_ESCAPE){
				bus.tx_state = NET_STATE_ESCAPING;
			}
			bus.tx_buff_cnt++;
		}else{
			uart0_writestop();
		}
	}else{
		//Not a valid state, stop sending.
		uart0_writestop();
	}	
}

//Should be called when we want to send a new message on the bus.
void uart0_writestart(void){
	if (bus.tx_state == NET_STATE_WAITING){
		bus.tx_state = NET_STATE_WRITING;
		if(bus.tx_buff_size - bus.tx_buff_cnt){
			USARTE0.DATA = bus.tx_buff[0];
			bus.tx_buff_cnt++;
		}else{
			uart0_writestop();
		}
	}
}

//Does nothing but set the state machine to start waiting
void uart0_writestop(void){
	bus.tx_state = NET_STATE_WAITING;
}

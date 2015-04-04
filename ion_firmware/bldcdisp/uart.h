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

#ifndef __UART__
#define __UART__

#include "../../lib_ion/bowbus.h"
extern bowbus_net_s bus;

void uart_init(void);
void uart0_init(void);
void uart0_writestart(void);
void uart0_writestop(void);
void uart_rate_find(void);

//TX PIN number.
#define TXPIN_0				PIN3_bm

//We don't want to try every frequency
#define BSEL_MIN	200
#define BSEL_MAX	220

//UART rate settings.
uint8_t bscale;
uint8_t bsel;


#endif

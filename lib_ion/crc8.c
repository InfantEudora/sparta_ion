/*
	Created: 6/23/2014 5:07:46 PM - Copyright (c) 
	Author: D. Prins			
		Infant - infant.tweakblogs.net
		mail: prinsje2004 at gmail

	File:		crc8.c
	Includes:	crc8.h				
				
		CRC8 implementation for BOWBus
	
	Based upon https://code.google.com/p/cywusb/source/browse/trunk/crc8.c?r=2
	 by Colin O'Flynn - Copyright (c) 2002
	 only minor changes by M.Thomas 9/2004

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

#include "crc8.h"

uint8_t crc8_bow( uint8_t *data_in, uint8_t len){
	uint8_t crc;
	uint8_t i,j,f;
	uint8_t data;

	crc = INIT;
	for (j = 0; j != len; j++){
		data = data_in[j];
		for(i = 8; i; i--) {
			f = ((crc ^ data) & 0x01);
			if (f == 0x01){
				crc = crc ^ POLY;
			}
			crc = (crc >> 1) & 0x7F;
			if (f == 0x01){
				crc = crc | 0x80;
			}
			data = data >> 1;
		}
	}
	return crc;
}

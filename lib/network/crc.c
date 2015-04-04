#include "crc.h"

/*
	CRC16-CCIT 
	Seeds:
	 Default = 0x1D0F
	 Xmodem  = 0x0000 <- SD Card uses this on the Data.

	 Others  = 0xFFFF
*/
uint16_t crc16_CCIT(uint8_t *data, uint16_t len, uint16_t crc)
{	
	while(len--){
		crc = ((uint8_t)(crc >> 8)) | (crc << 8);
		crc ^= *(data++);
		crc ^= ((uint8_t)crc) >> 4;
		crc ^= crc << 12;
		crc ^= (crc & 0xFF) << 5;
	}
	return crc;
}

/*
	Compute the Modbus CRC, seed with 0xFFFF.
*/
uint16_t crc16_Modbus(uint8_t *data, uint16_t len, uint16_t crc)
{
	while(len--){
		crc ^= *(data++);
		for (uint8_t i = 8; i != 0; i--) {    		// Loop over each bit
			if ((crc & 0x0001) != 0) {      		// If the LSB is set
				crc >>= 1;                    		// Shift right and XOR 0xA001
				crc ^= 0xA001;
			}else{                            		// Else LSB is not set
				crc >>= 1;                   		// Just shift right
			}
		}
	}	
	//Change the endianness.
  	return (crc>>8|((uint8_t)crc)<<8);  
}

/*
	Return CRC-8 of the data, using x^8 + x^2 + x + 1 polynomial.
	Used by LTC6803 as PEC byte.
*/
uint8_t crc8(uint8_t *data_in, uint8_t len){
	const uint8_t *data = data_in;
	uint16_t crc = 0xC0; // <- Apparently this is the initial value.
	int i, j;
	for (j = len; j; j--, data++) {
		crc ^= (*data << 8);
		for(i = 8; i; i--) {
			if (crc & 0x8000)
				crc ^= (0x1070 << 3);
			crc <<= 1;
		}
	}
	return (uint8_t)(crc >> 8);
}
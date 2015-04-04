/*
 * eeprom.c
 *
 * Created: 4/24/2014 7:50:29 PM
 *  Author: Dick
 */ 

#include <stdint.h>
#include <string.h> //Memcpy
#include "eeprom.h"

/*
	Writes data to an EEPROM block.
*/
bool eemem_write_block(uint16_t header, uint8_t* in, uint8_t len, uint8_t block){
	//32 bytes
	uint8_t block_mem[EEPROM_PAGESIZE];	//Memory for writing to block.
	uint8_t block_ver[EEPROM_PAGESIZE]; //Verification

	//Flush buffer just to be sure when we start.
	EEPROM_FlushBuffer();

	//Use memory mapped access.
	EEPROM_EnableMapping();
	
	//Create EEPROM buffer.
	block_mem[0] = (uint8_t)(header>>8);
	block_mem[1] = (uint8_t)(header);

	//Clear the block memory:
	for (uint8_t i=2;i<32;i++){
		block_mem[i] = 0;
	}

	//Save the settings: 
	if (len < 30){
		memcpy(&block_mem[2],in,len);
	}else{
		return 0;
	}	

	//Create a CRC
	uint16_t crc1 = crc16_CCIT(block_mem,30,0xFFFF);	
	block_mem[30] = (uint8_t)(crc1>>8);
	block_mem[31] = (uint8_t)(crc1);	

	//Clear eeprom page.
	EEPROM_WaitForNVM();
	EEPROM_ErasePage(block);

	EEPROM_WaitForNVM();
	//Write to eeprom
	for (uint8_t i=0;i<32;i++){
		EEPROM(block, i) = block_mem[i];
	}
	
	EEPROM_AtomicWritePage(block);
	
	//Load a single eeprom page.
	EEPROM_WaitForNVM();
	
	for (uint8_t i=0;i<EEPROM_PAGESIZE;i++){
		block_ver[i] = EEPROM(block,i);
		if (block_ver[i] != block_mem[i]){
			return false;
		}
	}
	
	return true;	
}

//Reads data from an EEPROM block. Returns false if header or CRC doesn't match.
bool eemem_read_block(uint16_t header, uint8_t* out, uint8_t len, uint8_t block){
	
	uint8_t block_mem[EEPROM_PAGESIZE];
	//Flush buffer just to be sure when we start.
	EEPROM_FlushBuffer();

	//Use memory mapped access.
	EEPROM_EnableMapping();
	
	//Read buffer.
	for (uint8_t i=0;i<32;i++){
		block_mem[i] = EEPROM(block, i);
	}
	
	//Verify header
	if (block_mem[0] != (uint8_t)(header>>8)){
		return false;
	}
	if (block_mem[1] != (uint8_t)(header)){
		return false;
	}
	
	//Do a CRC check:
	uint16_t crc1 = crc16_CCIT(block_mem,30,0xFFFF);	
	uint16_t crc1r;
	
	crc1r = block_mem[30]<<8;
	crc1r |= block_mem[31];
	
	//Compare
	if (crc1 != crc1r){	
		return false;
	}
	
	//Export the data:
	memcpy(out,(uint8_t*)&block_mem[2],len);
	
	return true;
}

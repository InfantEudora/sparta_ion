/*
 * eeprom.h
 *
 * Created: 4/24/2014 7:51:16 PM
 *  Author: Dick
 */ 


#ifndef EEPROM_H_
#define EEPROM_H_

//Includes the structures used in eeprom.

#include "../../lib/network/crc.h"
#include "eeprom_driver.h"

 bool eemem_write_block(uint16_t header, uint8_t* in, uint8_t len, uint8_t block);
 bool eemem_read_block(uint16_t header, uint8_t* in, uint8_t len, uint8_t block);



#endif /* EEPROM_H_ */
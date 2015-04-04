/*
 * Crc.h
 *
 *  Created on: Aug 2012
 *      Author: D. Prins
 *		Blablabla
 */

#ifndef __CRC__
#define __CRC__

#include <stdint.h>


uint16_t crc16_CCIT(uint8_t *data, uint16_t len, uint16_t crc);
uint16_t crc16_Modbus(uint8_t *data, uint16_t len, uint16_t crc);

#endif
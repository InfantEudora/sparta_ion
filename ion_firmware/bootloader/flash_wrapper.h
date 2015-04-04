/*
 * flash_wrapper.h
 *
 * Created: 5/15/2014 12:05:26 AM
 *  Author: Dick Prins
 
 */ 
#ifndef FLASH_WRAPPER_H_
#define FLASH_WRAPPER_H_

void EraseWriteAppPage(uint8_t pageAddress);
void ReadFlashPage(const uint8_t * data, uint8_t pageAddress);


#endif
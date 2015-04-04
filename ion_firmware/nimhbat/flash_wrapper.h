/*
 * flash_wrapper.h
 *
 * Created: 5/15/2014 12:05:26 AM
 *  Author: Dick
 */ 


#ifndef FLASH_WRAPPER_H_
#define FLASH_WRAPPER_H_

#define _BOOT_SECTION	__attribute__((__section__(".BOOT")))__attribute__((__used__))
_BOOT_SECTION bool save_settings(void);

_BOOT_SECTION void EraseWriteAppPage(uint8_t pageAddress);



#endif /* FLASH_WRAPPER_H_ */
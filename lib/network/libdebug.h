#ifndef _LIB_L_DEBUG_H_
#define _LIB_L_DEBUG_H_

#ifdef AVR

#else
#include <unistd.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdarg.h>
#include <stddef.h>

#include "../common/common.h"



#if (UARTDEBUG)
	//#warning "UARTDEBUG is on"
	extern uint8_t debug_uart_level;
	void printfdebug_uart(uint8_t clr,uint8_t lvl,const char* format, ... );
	void debug_uart(uint8_t lvl);
#else
	#define printfdebug_uart(a,l,...);
	#define debuguart(a);
#endif



#if (FILEDEBUG)
	//#warning "FILEDEBUG is on"
	extern uint8_t debug_file_level;
	void printfdebug_file(uint8_t clr,uint8_t lvl,const char* format, ... );
	void debug_file(uint8_t lvl);
#else
	//#warning "FILEDEBUG OFF"
	#define printfdebug_file(a,l,...);
	#define debugfile(a);
#endif






#endif
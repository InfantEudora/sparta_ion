#include "libdebug.h"

//All component's debuggin can be compiled in, or left out:
//#define UARTDEBUG		1

//
#if (UARTDEBUG)
	uint8_t debug_uart_level = 0;
	void printfdebug_uart(uint8_t clr,uint8_t lvl,const char* format, ... ){ 		
		if (debug_uart_level >= lvl){
			printf_color(clr);
			va_list arglist;
			va_start( arglist, format );
			vprintf( format, arglist );
			va_end( arglist );
			printf_color(CLR_CANCEL);
		}
	}

	void debug_uart(uint8_t lvl){
		debug_uart_level = lvl;
	}
#endif

#if (FILEDEBUG)
	uint8_t debug_file_level = 0;
	void printfdebug_file(uint8_t clr,uint8_t lvl,const char* format, ... ){ 	
		
		if (debug_file_level >= lvl){
			printf_color(clr);
			va_list arglist;
			va_start( arglist, format );
			vprintf( format, arglist );
			va_end( arglist );
			printf_color(CLR_CANCEL);
		}
	}

	void debug_file(uint8_t lvl){
		debug_file_level = lvl;
	}
#endif
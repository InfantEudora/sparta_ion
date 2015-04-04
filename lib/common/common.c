#include <stdio.h> 
#include <stdbool.h> 
#include "common.h"

void printf_hex(uint8_t* buff,uint8_t len, bool lf){
	for (uint8_t i=0;i<len;i++){		
		printf("%02X ",buff[i]);
	}
	if (lf){
		printf("\r\n");
	}
}

void printf_hex_block(uint8_t* buff,int len, bool lf){
	printf_color(CLR_GREEN);
	printf("%04X | ",0);
	printf_color(CLR_CANCEL);
	
	int r = 0;
	for (int i=0;i<len;i++){		
		printf("%02X ",buff[i]);
		r = i+1;
		if (r && (r%8 == 0)){
			printf(" ");
		}
		if (r && (r%16 == 0)){
			printf_color(CLR_GREEN);
			printf("\n%04X | ",r);
			printf_color(CLR_CANCEL);
		}
	}

	if (lf){
		printf("\n");
	}
}

void save_cursor(void){printf("\033[s");};
void restore_cursor(void){printf("\033[u");};

/*
	Printf color with arguments.
*/
void printf_clr(uint8_t clr,const char* format, ... ){   
    printf_color(clr);
	va_list arglist;
	va_start( arglist, format );
	vprintf( format, arglist );
	va_end( arglist );
    printf_color(CLR_CANCEL);
}

/*Change color output on terminal screen */
void printf_color(uint8_t clr){
	switch(clr){
		case CLR_BLACK:
			printf("\x1b[30m");
			break;
		case CLR_RED:
			printf("\x1b[31m");
			break;
		case CLR_GREEN:
			printf("\x1b[32m");
			break;
		case CLR_YELLOW:
			printf("\x1b[33m");
			break;
		case CLR_BLUE:
			printf("\x1b[34m");
			break;
		case CLR_MAGENTA:
			printf("\x1b[35m");
			break;
		case CLR_CYAN:
			printf("\x1b[36m");
			break;
		case CLR_WHITE:
			printf("\x1b[37m");
			break;
		case CLR_CANCEL:
		default:
			printf("\x1b[0m");
			break;
	}
}

/*
	Returns the difference in time between two timeval structures.
	These are normally in usec accuracy.
*/
#ifndef AVR
uint32_t time_diff_ms(struct timeval* begin, struct timeval* end){
	uint32_t msec;
	if (begin->tv_sec > end->tv_sec){
		return 0;
	}else{
		msec  = (end->tv_sec-begin->tv_sec)*1000;
		msec += (end->tv_usec-begin->tv_usec)/1000; 
	}
	return msec;
}

uint64_t time_diff_us(struct timeval* begin, struct timeval* end){
	uint64_t usec;
	if (begin->tv_sec > end->tv_sec){
		return 0;
	}else{
		usec  = (end->tv_sec-begin->tv_sec)*1000LL*1000LL;
		usec += (end->tv_usec-begin->tv_usec); 
	}
	return usec;
}

int file_copy(char* input, char* output, int out_permission){
	int in_fd = open(input, O_RDONLY);
	if (in_fd == -1){
		return -1;
	}
	//printf("in_fd: %i\n",in_fd);

	int out_fd = open(output, O_WRONLY | O_CREAT, out_permission);
	if (out_fd == -1){
		return -1;
	}
	//printf("out_fd: %i\n",out_fd);
	
	char buf[8192];
	int cnt = 0;


	while (1) {
		ssize_t result = read(in_fd, &buf[0], sizeof(buf));
		//printf("result: %i\n", result);
		if (!result){
			//Done.
			close(in_fd);
			close(out_fd);
			return 1;
		}

		//Don't loop forever.
		cnt++;
		if (cnt > 100){
			return -1;
		}
		
		write(out_fd, &buf[0], result);
	}


	return -1;
}


//Safe getline from buffer.
int sngetline(char* in, char* out, ssize_t max){
	ssize_t size = 0;
	while(*in){
		size++;
		if (*in == '\r'){
			if (size < max){
				if (in[1] == '\n'){
					size++;
				}
			}
			break;
		}
		if (*in == '\n'){
			break;
		}
		if (size == max){
			break;
		}
		*out++ = *in;
		in++;
	}

	return size;
}



#endif

void bswap16(uint16_t* in, uint16_t* out){
	uint16_t uin = *in;
	uint16_t temp;
	temp = (uin >> 8) | (((uint8_t)uin)<<8);
	*out = temp;
}

#define bswap_16(x) \
({ \
	uint16_t __x = (x); \
	((uint16_t)( \
		(((uint16_t)(__x) & (uint16_t)0x00ffU) << 8) | \
		(((uint16_t)(__x) & (uint16_t)0xff00U) >> 8) )); \
})

/*
*/
uint8_t ascii_hex_to_8(uint8_t in){
	uint8_t temp = 0;
	if ((in >= 0x30) && (in <= 0x39)){
		temp = in-0x30;
	}
	if ((in >= 0x41) && (in <= 0x46)){
		temp = in-0x37;
	}
	return temp;
}



/*Swaps it around*/
void floatfrommemswapped(float* f_out,uint8_t* mem){
	float f;
	uint8_t* tmem;
	tmem = (uint8_t*)&f;
	
	tmem[0] = mem[3];
	tmem[1] = mem[2];
	tmem[2] = mem[1];
	tmem[3] = mem[0];
	
	*f_out = f;
}

/*Swaps it around*/
void int16frommemswapped(int16_t* i_out,uint8_t* mem){
	int16_t i;
	uint8_t* tmem;
	tmem = (uint8_t*)&i;
	
	tmem[0] = mem[1];
	tmem[1] = mem[0];
	
	*i_out = i;
}

//Application wide variables: Debug levels:
int debug_level_tcpser = 1;
int debug_level_tcpclient = 0;
int debug_level_tcpcrypt = 1;

//Output printing:
int source_out_tcpser = 1;
int source_out_tcpclient = 0;
int source_out_tcpcrypt = 1;


//The one printf statement to rule them all
//Color can be mixed in the source, or a level
int tcp_printf(uint32_t source,uint16_t debug_level,const char* format, ...){
	int len = 0;
	//Default color:
	printf_color(CLR_CANCEL);

	//Print a color if we want one.
	if (source & SRC_COLOR_MASK){
		printf_color(source & SRC_COLOR_MASK);
	}

	//Levels
	if (source & SRC_LEVEL_MASK){
		if ((source & SRC_LEVEL_MASK) == LEVEL_INFO){
			printf_color(CLR_CANCEL);
		}else if ((source & SRC_LEVEL_MASK) == LEVEL_WARNING){
			printf_color(CLR_YELLOW);
		}else if ((source & SRC_LEVEL_MASK) == LEVEL_ERROR){
			printf_color(CLR_RED);
		}else if ((source & SRC_LEVEL_MASK) == LEVEL_CRITICAL){
			printf_color(CLR_RED);
		}
	}



	//May we output?
	switch(source & SRC_SOURCE_MASK){
		case SRC_TCPSERV:
			if (debug_level > debug_level_tcpser){
				return len;
			}
			if (source_out_tcpser < 1){
				return len;
			}
			break;
		case SRC_TCPCLIENT:
			if (debug_level > debug_level_tcpclient){
				return len;
			}
			if (source_out_tcpclient < 1){
				return len;
			}
			break;
		case SRC_TCPCRYPT:
			if (debug_level > debug_level_tcpcrypt){
				return len;
			}
			if (source_out_tcpcrypt < 1){
				return len;
			}
			break;
		case SRC_TCPLOG:
			if (debug_level > debug_level_tcpser){
				return len;
			}
			if (source_out_tcpser < 1){
				return len;
			}
			break;
		
		default:
			printf("PRINTF: Unknown source.\n");
			break;
	}


	

	//Actually print the thing:
	va_list argptr;
	va_start(argptr, format);
	len +=vfprintf(stdout, format, argptr);
	va_end(argptr);

	//Restore color
	printf_color(CLR_CANCEL);


	return len;
}


//Clear the terminal.
void term_clear(void){	
	printf("\x1b[2J\x1b[1;1H");	
}
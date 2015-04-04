/*
 * Crc.h
 *
 *  Created on: June 2013
 *      Author: D. Prins
 *	
 *		Defines for commonly used functions.
 *		
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#ifndef AVR

#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>

 //Some forward declarations to make the compiler stop nagging.
//int usleep(int);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);
void exit(int);
int close(int);
/*
void cfmakeraw(struct termios *termios_p);
int kill(pid_t pid, int sig);
FILE *popen(const char *command, const char *type);
int pclose(FILE *stream);
*/
char *strdup(const char *s1);


//Own functions
void printf_hex(uint8_t* buff,uint8_t len, bool lf);


void int16tomem(int16_t* int16, uint8_t* mem);
void uint16tomem(uint16_t* uint16, uint8_t* mem);

void floatfrommem(float* f_out,uint8_t* mem);

void bswap16(uint16_t* in, uint16_t* out);

uint32_t time_diff_ms(struct timeval* begin, struct timeval* end);

uint8_t ascii_hex_to_8(uint8_t in);

int file_copy(char* input, char* output, int out_permission);

void floatfrommemswapped(float* f_out,uint8_t* mem);
void int16frommemswapped(int16_t* i_out,uint8_t* mem);

int tcp_printf(uint32_t source,uint16_t debug_level,const char* format, ...);

//Terminal stuff:
void save_cursor(void);
void restore_cursor(void);
void term_clear(void);
void printf_hex_block(uint8_t* buff,int len, bool lf);


#define BYTETOBINARYPATTERN "%d%d%d%d%d%d%d%d"
#define BYTETOBINARY(byte)  \
  (byte & 0x80 ? 1 : 0), \
  (byte & 0x40 ? 1 : 0), \
  (byte & 0x20 ? 1 : 0), \
  (byte & 0x10 ? 1 : 0), \
  (byte & 0x08 ? 1 : 0), \
  (byte & 0x04 ? 1 : 0), \
  (byte & 0x02 ? 1 : 0), \
  (byte & 0x01 ? 1 : 0) 



//Application wide variables: Debug levels:
extern int debug_level_tcpser;
extern int debug_level_tcpclient;
extern int debug_level_tcpcrypt;

//Output printing:
extern int source_out_tcpser;
extern int source_out_tcpclient;
extern int source_out_tcpcrypt;





int sngetline(char* in, char* out, ssize_t max); //Safe getline.




#endif

void printf_color(uint8_t clr);
void printf_clr(uint8_t clr,const char* format_, ... );

//Print suurces
#define SRC_SOURCE_MASK 0x00FF00

#define SRC_TCPSERV		0x000100
#define SRC_TCPCLIENT 	0x000200
#define SRC_TCPCRYPT	0x000300
#define SRC_TCPLOG		0x000400

#define SRC_CONSOLE		0x000F00

#define SRC_COLOR_MASK	0x0000FF

#define SRC_LEVEL_MASK	0xFF0000

#define LEVEL_INFO		0x010000		
#define LEVEL_WARNING	0x020000
#define LEVEL_ERROR		0x030000
#define LEVEL_CRITICAL	0x040000
#define LEVEL_INPUT		0x050000
#define LEVEL_OUTPUT	0x060000

#define LINE_FEED		true
#define NO_LINEFEED		false

#define CLR_BLACK		1
#define CLR_RED			2
#define CLR_GREEN		3
#define CLR_YELLOW		4
#define CLR_BLUE		5
#define CLR_MAGENTA		6
#define CLR_CYAN		7
#define CLR_WHITE		8
#define CLR_CANCEL		9


#endif //ndef H
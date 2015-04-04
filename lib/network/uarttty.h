#ifndef _UART_TTYH_
#define _UART_TTYH_

#include <stdio.h> 			// standard input / output functions
#include <string.h> 		// string function definitions
#include <unistd.h> 		// UNIX standard function definitions
#include <fcntl.h> 			// File control definitions
#include <errno.h> 			// Error number definitions
#include <termios.h> 		// POSIX terminal control definitionss
#include <time.h>   		// time calls
#include <sys/select.h>		// Select
#include <stdint.h>			// int type defs.
#include <sys/ioctl.h>

#include "../common/common.h"
#include "fifo.h"
#define TIOCEXCL        0x540C



typedef struct uart_s uart_s;

struct uart_s{
	int fd;					//File descriptor
	speed_t rate;			//Baud rate for the port
	char* name;
	bool opened;			//If it's previously opened or not.	

	fifo_t *rxfifo;
	fifo_t *txfifo;
	void 	(*write_start)(uart_s*);
	void 	(*write_stop)(uart_s*);
};

void	uart_list_devs(void);

void 	uart_init(uart_s* uart);
int 	uart_open(uart_s* uart);

void 	uart_write_start(uart_s* uart);
void 	uart_write_stop(uart_s* uart);

int 	uart_close(uart_s* uart);
int 	uart_setdevice(uart_s* uart,char* devname);

int 	uart_configure(uart_s* uart, speed_t rate, bool parity);
int 	uart_write(uart_s* uart,uint8_t* data,int len);

int 	uart_read_start(uart_s* uart, int timeout);
int 	uart_read(uart_s* uart, uint8_t* buff,int lenmax, int maxms);
void 	uart_flush_read(uart_s* uart);

#endif
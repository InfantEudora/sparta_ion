/*
	Serial Port interface on a Unix environment.
*/

#include "uart.h"

#ifdef AVR

void uart_init(uart_s* uart){	
	uart->transmitting = false;

	uart->rxfifo = NULL;
	uart->txfifo = NULL;

	//Here they are attached to one function, and switch device with the uart argument.
	//On a uC, manually attach the start/stop functions.
	uart->write_start = NULL;
	uart->write_stop = NULL;	
}

#else

#define ERROR_NONE			0
#define ERROR_NO_UART 		1
#define ERROR_PORT_BUSY 	2
#define ERROR_DATA_LEN 		3
#define ERROR_TIMEOUT		4

void uart_init(uart_s* uart){
	memset(uart,0,sizeof(uart_s));
	uart->fd = 0;
	uart->rate = B57600;
	uart->opened = false;

	uart->rxfifo = NULL;
	uart->txfifo = NULL;

	//Here they are attached to one function, and switch device with the uart argument.
	//On a uC, manually attach the start/stop functions.
	uart->write_start = &uart_write_start;
	uart->write_stop = &uart_write_stop;
}

//Write to uart
int uart_write(uart_s* uart,uint8_t* data,int len){
	if (uart == NULL){
		return ERROR_NO_UART;
	}
	if (uart->fd == -1){
		return ERROR_PORT_BUSY;
	}	

	//int status = TIOCM_CTS;
	//ioctl (uart->fd, TIOCMBIS, &status);

	//Send message to the meter.
	int written = write(uart->fd, data, len);

	//Wait for characters to be sent.
	//fsync(uart->fd);

	//Ioctl?
	//ioctl (uart->fd, TIOCMBIC, &status);
}

void uart_write_stop(uart_s* uart){
	uart_printf(CLR_CANCEL,1,"Uart stopped writing.\n");

	if (uart){
		if (uart->opened){
			//uart_close(uart);
		}
	}
	
}

//Reads data into uart's fifo.
int uart_read_start(uart_s* uart, int timeout){
	if (!uart->rxfifo){
		uart_printf(CLR_CANCEL,0,"Uart has no rxfifo\n");
		return -1;
	}

	uart_printf(CLR_CANCEL,2,"uart_read_start\n");
	if (!uart->opened){
		uart_open(uart);
	}

	//Wiggle
	/*

	int status = TIOCM_CTS;
	ioctl (uart->fd, TIOCMBIS, &status);
	ioctl (uart->fd, TIOCMBIC, &status);
	*/

	//Use a buffer to flatten the fifo.
	uint8_t* buff = (uint8_t*)malloc(uart->rxfifo->size);

	int n = uart_read(uart,buff,512,timeout);
	//Some debug data:
	uart_printf(CLR_CANCEL,1,"Received %i characters\n",n);
	#if(UARTDEBUG)
	if (n){
	printf_hex_block(buff,n,1);
	}
	#endif

	if (n == -1){
		free(buff);
		return -1;
	}

	//Store in fifo.
	int i =0;
	int r=n;
	while(r--){		
		fifo_putc(uart->rxfifo,buff[i]);
		i++;
	}
	uart_printf(CLR_CANCEL,1,"Done\n");
	free(buff);

	return n;
}

//Interface between fifo and uart_write
void uart_write_start(uart_s* uart){	
	if (!(uart->txfifo)){
		uart_printf(CLR_CANCEL,0,"Uart has no txfifo.\n");
		return;
	}
	
	//Use a buffer to flatten the fifo.
	uint8_t* buff = (uint8_t*)malloc(uart->txfifo->size);
	//Empty the fifo
	int i = 0;
	while(fifo_get(uart->txfifo,&buff[i])){
		i++;
	};
	
	uart_printf(CLR_CANCEL,0,"uart_write_start (len %i): \n",i );
	#if(UARTDEBUG) 
	printf("Written:\n");
	printf_hex_block(buff,i,1);
	printf("\n");
	#endif
	
	//Write em out:
	uart_write(uart,buff,i);	
	
	//free buffer.
	free(buff);
}

//Read a max amount of characters into a buffer.
int uart_read(uart_s* uart, uint8_t* buff, int lenmax, int maxms){
	if (uart == NULL){
		return ERROR_NO_UART;
	}
	if (!uart->opened){
		return 0;
	}
	uart_printf(CLR_CANCEL,2,"uart_read\n");

	int n = 0;
	int rlen = 0;
	int number_of_bytes_read=0;
	struct timeval timeout, holdthis;	
	// initialise the timeout structure
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000UL*(uint32_t)maxms; //Wait 200ms
	//timeout.tv_usec = 5000; 

	struct timeval start, now;

	holdthis=timeout;

	int read_size = lenmax;
	
	//Wait for characters to be sent.
	//fsync(uart->fd);
	
	if (uart->fd == -1){
		return -1;
	}

	//Set for reading.
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(uart->fd, &readfds);


	uint32_t diffms;
   	gettimeofday (&start, NULL);


	while(rlen < lenmax){
		n = select(uart->fd + 1, &readfds, NULL, NULL, &timeout);
		uart_printf(CLR_CANCEL,1,"Selecting to %i, lenmax %i, rlen %i, rsz: %i\n",maxms,lenmax,rlen,read_size);
		
		if(n>0){

			number_of_bytes_read = read(uart->fd,&buff[rlen],read_size);	//This starts writing at bufferout[0].
			//scc_printf(SRC_TCPSERV|LEVEL_WARNING,0,"\x1b[37m%d bytes selected in %dusec, appended at: %d, last was '%02X',\r\n" , number_of_bytes_read, holdthis.tv_usec-timeout.tv_usec, rlen, buffer_out[rlen+number_of_bytes_read-1]);
			rlen += number_of_bytes_read;
			read_size -= number_of_bytes_read;

			uart_printf(CLR_CANCEL,1,"n>0: Read %i/%i\n",rlen,lenmax);
			
			//Set the new timeout, or return:
			gettimeofday (&now, NULL);
			diffms = time_diff_ms(&start,&now);
			//printf("Diff: %lu n==%i\n",diffms,n);
			if (diffms > maxms){
				//uart_close(uart);
				return rlen;
			}
			timeout.tv_sec = 0;
			timeout.tv_usec = 1000L*(int32_t)(maxms-diffms);

			uart_printf(CLR_CANCEL,1,"remaining : %lu\n",timeout.tv_usec);
			timeout=holdthis;
			
		}else if(n == 0){
			//Not all characters received.
			//Set the new timeout, or return:
			gettimeofday (&now, NULL);
			diffms = time_diff_ms(&start,&now);
			uart_printf(CLR_CANCEL,1,"Diff: %lu. n==0\n",diffms);
			//uart_close(uart);
			return rlen;
		}else{
			//Port may be occupied.
			//Set the new timeout, or return:
			gettimeofday (&now, NULL);
			diffms = time_diff_ms(&start,&now);
			uart_printf(CLR_CANCEL,1,"Diff: %lu n== -1\n",diffms);
			//uart_close(uart);
		 	return 0;
		}

		
	}


	uart_printf(CLR_CANCEL,1,"Done.\n");
	//uart_close(uart);
	return rlen;
}



/*
	Clears the read buffer.
*/
void uart_flush_read(uart_s* uart){
	uint8_t buffer[64];
	int nbytes=1;

	if (!(uart->opened)){
		//Let's do that first.
		if (!uart_open(uart)){
			return;
		}
	}


	struct timeval timeout;	
	// initialise the timeout structure
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;

	// Update File Descriptor so that it's correct as the state of the program changes.
	while(1){

		fd_set 	readfds; 	//Do i want this in a uart_object? Or just leave it?
		FD_ZERO(&readfds);			//Clear the set of file_descriptors. 
		FD_SET(uart->fd, &readfds);	//Add 'tty_fd' to 'readfds' set of file_descriptors.
		int n = select(uart->fd + 1, &readfds, NULL, NULL, &timeout);
		if(n>0){		
			nbytes = read(uart->fd,buffer,64);	//This starts writing at bufferout[0].
			uart_printf(CLR_CANCEL,0,"Number of bytes cleared: %i\n",nbytes);
			if (nbytes == 0){
				//This can happen when the port becomes unavailable.
				uart_printf(CLR_CANCEL,0,"Port disconnected\n");
				uart_close(uart);
				return;
			}			
		}else{
			//Let me know.
			//scc_printf(SRC_TCPSERV|LEVEL_WARNING,0,"No bytes where cleared.\n");
			return;
		}	
	}
}


int uart_setdevice(uart_s* uart,char* devname){
	if (!uart){
		return -1;
	}

	if (uart->name){
		free(uart->name);		
	}

	uart->name = malloc(strlen(devname));
	strcpy(uart->name,devname);

	return 1;
}

/*
	The device name is specified in uart->name
*/
int uart_open(uart_s* uart){
	if (uart == NULL){
		return 0;
	}

	uart->fd = open(uart->name, O_RDWR | O_NOCTTY | O_NDELAY );	
	if(uart->fd == -1){	
		uart_printf(CLR_RED,0,"Unable to open tty: [%s]\n",uart->name);
		uart->opened = false;
		return 0;
	}else{
		fcntl(uart->fd, F_SETFL, 0);
		uart_printf(CLR_CANCEL,0,"Uart %s is openened",uart->name);
		int r = ioctl(uart->fd, TIOCEXCL);
		if (r>-1){
			uart_printf(CLR_CANCEL,0,"with exclusive access.\n");
		}else{
			uart_printf(CLR_CANCEL,0,"\n");
		}		
	}	
	uart->opened = true;
	return(uart->fd);
}

int uart_close(uart_s* uart){
	if(uart->fd){		
		close(uart->fd);
		uart->opened = false;
		uart_printf(CLR_CANCEL,0,"Uart %s is closed\n",uart->name);

	}
	uart->opened = false;
	return(uart->fd);
}


int uart_configure(uart_s* uart, speed_t rate, bool parity){
	struct termios options ;
	int     status ;

	if (uart == NULL){
		return 0;
	}

	//Is the port already open?
	if (!(uart->opened)){
		if (!(uart_open(uart))){
			return 0;
		}
	}

	fcntl (uart->fd, F_SETFL, O_RDWR) ;

	// Get and modify current options:
	tcgetattr (uart->fd, &options) ;
	cfmakeraw   (&options);
	cfsetispeed(&options, rate);
	cfsetospeed(&options, rate);
	
	

    options.c_cflag |= (CLOCAL | CREAD) ;
    if (!parity){
   	 	options.c_cflag &= ~PARENB ;
	}else{
		options.c_cflag |= PARENB ;
	}
    options.c_cflag &= ~CSTOPB ;
    options.c_cflag &= ~CSIZE ;
    options.c_cflag |= CS8 ;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | IEXTEN | ISIG) ;

    options.c_oflag = 0;

    options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                | INLCR | IGNCR | ICRNL | IXON);
	
	//options.c_cflag &= ~CRTSCTS;

    options.c_cc [VMIN]  =   1 ;
    options.c_cc [VTIME] = 0 ;	// Ten seconds (100 deciseconds)

	tcsetattr (uart->fd, TCSAFLUSH, &options) ;

	

	ioctl (uart->fd, TIOCMGET, &status);

	//status |= TIOCM_DTR ;
	//status |= TIOCM_RTS ;

	/*
	One of the RS232 status lines has changed (USB-Serial chips only). A change of level (high
	or low) on CTS# / DSR# / DCD# or RI# will cause it to pass back the current buffer even
	though it may be empty or have less than 64 bytes in it.

	*/

	ioctl (uart->fd, TIOCMSET, &status);


	
	uart_printf(CLR_CANCEL,0,"Uart %s opened at rate [%u]\n",uart->name, rate);

	return uart->fd;
}



#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

//Magic.
void uart_list_devs(void){
   	const char* directory = "/dev/";
   	DIR* dir = opendir(directory);
   	if (dir){
		struct dirent* de = 0;

		while ((de = readdir(dir)) != 0){
			if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
				continue;
		
			int pid = -1;
			int res = sscanf(de->d_name, "%d", &pid);
			char* instr = strstr(de->d_name, "tty");
			if (instr){
				uart_printf(CLR_CANCEL,0,"Reading dir: %s \n",de->d_name);
				char filename[256] = {0};
				sprintf(filename, "%s/%s", directory, de->d_name);
				int fd = open (filename, O_RDWR | O_NONBLOCK);
				if (fd != -1){
					int serinfo;
					if (ioctl (fd, TIOCMGET, &serinfo) != -1){
						uart_printf(CLR_CANCEL,0,"Must be a serial device.\n");
					}
					close(fd);
				}
			}
		}
		closedir(dir); 
	}
}

#endif
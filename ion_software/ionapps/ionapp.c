#include <stdio.h>
#include <stdint.h>

#define UARTDEBUG 	1
#define FILEDEBUG	1
#include "../../lib/network/libdebug.h"

#include "../../lib/common/common.h"
#include "../../lib/network/uart.h"
#include "../../lib/network/stream.h"

#include "../../lib_ion/bowbus.h"

//Stuff:
#define UART_BUFFER_SIZE	512
uint8_t rxbuffer[UART_BUFFER_SIZE];
uint8_t txbuffer[UART_BUFFER_SIZE];
fifo_t txfifo;
fifo_t rxfifo;
uart_s uart[1];
stream_t stream[1];

bowbus_net_s bus[1];



bool app_openuart(void);

void parse_userinput(uint8_t *buf){
	uint8_t input[256] = {0};	
	int i1;

	//These work everywhere.
	sscanf(buf,"%s %i",input,&i1);
	if (strcmp(input,"clear") == 0){
		term_clear();
		return;
	}else if (strcmp(input,"debug") == 0){
		return;
	}else if (strcmp(input,"open") == 0){
		app_openuart();
		return;
	}
	printf("Unknown command.\n");
}

bool app_openuart(void){
	//Open a uart
	uart_init(uart);
	uart_setdevice(uart,"/dev/ttyS41");
	if (!uart_configure(uart,B9600,false)){
		//Failed to open.
		printf_clr(CLR_YELLOW,"Try a different tty:?\n");
		uint8_t newname[64];
		fgets(newname,64,stdin);
		//Remove \n
		int len = strlen(newname);	
		newname[len-1] = 0;		

		uart_setdevice(uart,newname);
		if (!uart_configure(uart,B9600,false)){
			printf_clr(CLR_RED,"Nope.\n");
			return false;
		}
	}

	//Setup everything:
	fifo_init(&txfifo,txbuffer,UART_BUFFER_SIZE);
	fifo_init(&rxfifo,rxbuffer,UART_BUFFER_SIZE);
	//Attach fifo's to uart.
	uart->rxfifo = &rxfifo;
	uart->txfifo = &txfifo;

	printf("Attatching funcions to stream\n");
	stream_init(stream,uart);
	stream->write_start = &stream_writestart;
	stream->write_stop = &stream_writestop;

	bus_init(bus);
}


bool bus_check_for_message(bowbus_net_s* bus){
	if (bus->new_mesage){		
		if (bus_parse(bus,bus->msg_buff,bus->msg_len)){			
			bus->msg_len = 0;
			bus->new_mesage = false;
			return true;
		}else{
			bus->msg_len = 0;
			bus->new_mesage = false;
			return false;
		}
	}
	return false;
}


//Handle all application input
#define STATE_READ 	0
#define STATE_ESC	1
#define STATE_CSI	2

#define INPUT_BUFFER_MAX	256

typedef struct termstate_s termstate_s;

struct termstate_s{
	char buffer[INPUT_BUFFER_MAX];
	int inpptr;
	char state;
};

termstate_s user_input;


void term_handle_character(termstate_s* term, char* c){
	bool linebreak = false;

	if (term->inpptr >= (INPUT_BUFFER_MAX -1)){
		term->inpptr = INPUT_BUFFER_MAX-1;
	}

	printf("inpptr = %i\n",term->inpptr );
	printf("state  = %i\n",term->state );

	if (term->state == STATE_READ){
		if (*c == 0x1B){
			term->state = STATE_ESC;
		}else if ((*c == 8)||(*c == 127)) { //Bullshit character
			if (term->inpptr > 0){
				term->inpptr--;
				term->buffer[term->inpptr] = 0;						
			}
		}else if ((*c == '\r')||(*c == '\n')) {
			//term->buffer[term->inpptr++] = 0;
			//printf("\x1b[%i;%iH",x,y);
			//fflush(stdout);	
			linebreak = true;
			
			//HANDLE IT!
			
			parse_userinput(term->buffer);
			memset(term->buffer,0,sizeof(term->buffer));
			term->inpptr = 0;
			

			term->inpptr = 0;
			term->buffer[term->inpptr] = 0;
		}else{
			//terminal_line();
			printf("CH %u\n",*c);
			//fflush(stdout);
			term->buffer[term->inpptr++] = *c;
			term->buffer[term->inpptr] = 0;

		}
	}else if (term->state == STATE_ESC){
		if (*c == 0x5B){
			term->state = STATE_CSI;
		}else{
			term->state = STATE_READ;
			//Escape button was pressed.
			//menu = MENU_MAIN;
			//show_help();
		}
	}else if (term->state == STATE_CSI){
		
		/*if (*c == 'A'){ //Up arrow
			if (menu==MENU_GARAGE){
				garage_input(INP_DOWN);
			}else{	
				//Load previous command
				cmd_hist_up(input);
				term->inpptr = strnlen(input,MAX_UI_BUFFERSIZE);
			}					
		}else if (*c == 'B'){ //Down arrow
			if (menu==MENU_GARAGE){
				garage_input(INP_UP);
			}
		}else if (*c == 'C'){//Right					
			if (menu==MENU_GARAGE){
				garage_input(INP_RIGHT);
			}
		}else if (*c == 'D'){ //Left
			if (menu==MENU_GARAGE){
				garage_input(INP_LEFT);
			}
		}else{
			printf("Escape!");
		}*/
		term->state = STATE_READ;
	}
}

//Read from STDIN with select: Set terminal to unbuffered, do it, and set it back.
void select_term(void){
	#define STDIN 0
	#define STDOUT 1
	//Current terminal settings and original settings.
	struct termios term, term_orig;

	//Get it.
	if(tcgetattr(0, &term_orig)){
		printf("tcgetattr failed\n");
		return;
	}

	term = term_orig;

	//Clear the canonical flag, and the echo flag so we don't have to wait for a \n character.
	//And we can dump the typed input anywhere.
	term.c_lflag &= ~ICANON;
	term.c_lflag &= ~ECHO;
	term.c_cc[VMIN] = 0;
	term.c_cc[VTIME] = 0;

	//Set it.
	if (tcsetattr(0, TCSANOW, &term)) {
		printf("tcsetattr failed\n");
		return;
	}

	char ch[1];
	
	fd_set rcvfds;
	struct timeval timeout;	
	timeout.tv_sec = 0;
	timeout.tv_usec = 50*1000; //Convert to ms


	while(1){
		FD_ZERO(&rcvfds);		
		FD_SET(STDIN, &rcvfds);	

		int n = select(STDIN + 1, &rcvfds, NULL, NULL, &timeout);

		//Fix the timeout 
		timeout.tv_usec = 50*1000; //Convert to ms

		if(n>0){
			//There are characters in stdin:
			int rc = read(STDIN, ch, 1);
			if(rc < 0){
				//Failed to read from STDIN:
				printf("Failed to read from stdin.\n");
				break;
			}else if (rc == 0){
				break;				
			}else{
				//Reading went ok:
				//write(STDOUT,ch,1);
				printf("Handling char\n");
				//Do something with this single character:
				term_handle_character(&user_input,ch);
			}
			//There is more...
		}else if(n == 0){
			//printf("STDIN timed out\n");
			break;			
		}else{ //-1
			//It's broken:
			printf("FD to STDIN is broken.\n");
			break;
		}

		//Print it in a bar of some kind:
		//pthread_mutex_lock(&console_lock);
		//save_cursor();

		//update_head();
		
		//Jump to the bottom of the screen:
		//printf("\x1b[%i;0H",term_size.ws_row - 2);
		//Ansi voodoo
		/*
		printf("\x1b[0J");
		printf("\x1b[32;100m");	
		printf("\x1b[2K");		//Clear line
		printf("\x1b[37;100m");	
		printf("> ");
		printf("\x1b[32;100m");	
		*/
		//Print the shit we just typed:
		//printf("cmd: %s",input);		

		
	}

	//Set it.
	if (tcsetattr(0, TCSANOW, &term_orig)) {
		printf("tcsetattr failed\n");
		return;
	}

}


int main(int argc, char *argv[]){
	
	term_clear();

	//Test
	parse_userinput("open");

	user_input.inpptr = 0;


	


	while(1){
		

		//Print state
		term_clear();

		printf("Input: %s\n", user_input.buffer);

		printf("Motor:\n");
		printf("  Speed    : %hu\n"    ,motor.speed);
		printf("  Mode     : %u\n"      ,motor.mode);
		printf("  Throttle : %u\n"  ,motor.throttle);
		printf("  Brake    : %u\n"     ,motor.brake);
		printf("  Status   : %u\n"    ,motor.status);		
		printf("  Voltage  : %hu\n"  ,motor.voltage);
		printf("  current  : %hi\n"  ,motor.current);

		printf("Display:\n");
		printf("  Distance : %lu\n"    ,display.distance);
		printf("  Speed    : %hu\n"    ,display.speed);
		printf("  SOC      : %u\n"       ,display.soc);
		printf("  Throttle : %u\n"  ,display.throttle);

		

		int n = uart_read_start(uart,20);

		//Read it:
		int len = 20;
		uint8_t buffer[20];
		int size = stream_read(stream,buffer,len);
		
		
		
		//Parse the data:
		for (int i=0;i<size;i++){
			bus_receive(bus,buffer[i]);

		}

		//Message?
		if (bus_check_for_message(bus)){
			printf_clr(CLR_GREEN,"MSG OK\n");
		}else{
			printf_clr(CLR_YELLOW,"MSG  NOT OK\n");
			printf("Read %i bytes into buffer.\n",size );
			printf_hex_block(buffer,size,true);
		}

		select_term();

	}



	return 0;
}
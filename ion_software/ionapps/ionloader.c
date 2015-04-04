#include <stdio.h>
#include <stdint.h>


#include "../../lib/network/libdebug.h"

#include "../../lib/common/common.h"
#include "../../lib/network/uart.h"
#include "../../lib/network/stream.h"
#include "../../lib/network/message.h"
#include "../../lib/network/file.h"


#define APPDEBUG		1

#if (APPDEBUG)
 #define debprintf(a,l,...) printf_clr(a,__VA_ARGS__)
 //#define debprintf(a,l,...) printf(__VA_ARGS__)
#else
 #define debprintf(a,l,...)
#endif

//Stuff:
#define UART_BUFFER_SIZE	512
uint8_t rxbuffer[UART_BUFFER_SIZE];
uint8_t txbuffer[UART_BUFFER_SIZE];
fifo_t txfifo;
fifo_t rxfifo;
uart_s uart[1];
stream_t stream[1];

//Firmware file.
hex_file_s filefw;

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



#define BOOT_WAIT_REQ	0
#define BOOT_WAIT_ACK	1
#define BOOT_WRITE		2
#define BOOT_CHECK		3
#define BOOT_FINISH		4

#define BOOT_WAIT_WRITE 5

struct {
	uint8_t state;			//State we are in.
	uint8_t address;		//Address we are writing to.	
	uint16_t block;			//Block we are working on.
	uint16_t offset;		//Chunk of block.
	uint16_t crc;			//CRC of current block.
	uint32_t firmwareoffs;	//Firmwareoffset.
	uint32_t size;			//Firmware total size.	
	uint32_t todo;			//Firmware total size to do
}bootstate;

typedef struct hwinfo_t hwinfo_t;
struct hwinfo_t{
	uint16_t version;
	uint8_t boot_address;
	uint8_t dev_address;
};

typedef struct fwinfo_t fwinfo_t;
struct fwinfo_t{
	uint16_t version;
	uint16_t size;
	uint16_t crc;
};

hwinfo_t hwinfo;
fwinfo_t fwinfo;

void select_term(void);

bool app_openuart(void);
void app_get_check(uint8_t address);
void app_get_info(uint8_t address);
void app_load_firmware(uint8_t address);
void app_prepare_bootload(uint8_t start_addr);
net_error_e app_parsemessage(fifo_t* fifo);
net_error_e app_handlemessage(uint8_t* data,msg_info_t* info);

void app_init_bootstate(void){
	memset(&bootstate,0,sizeof(bootstate));
	bootstate.address = 4;
}

void app_print_info(void){
	printf("Firmware loader for OpenSource Sparta Ion Hardware. You can type the following:\n\n");
	
	printf("start        - Put all devices in bootload mode.\n");
	printf("startmotor   - Put only the motor in bootload mode. \n");
	printf("loadbutton   - Load button PCB. (firmware/button.hex)\n");
	printf("loadmotor    - Load motor PCB. (firmware/motor.hex)\n");
	printf("motorhex     - Only open the hexfile.\n");
	printf("info3        - Get bootinfo from button.\n");
	printf("info4        - Get bootinfo from motor.\n");

	printf("\n");
	printf("Steps for uploading motor firmware:\n");
	printf(" 1 - Make sure motor.hex is the correct file that you want to upload.\n");
	printf(" 2 - Remove power from motor.\n");
	printf(" 3 - Connect OneWire bus to PC, or whatever this is currently running on.\n");
	printf(" 4 - type 'startmotor', it will tell the motor to stay in bootload mode.\n");
	printf(" 5 - type 'loadmotor', it will upload motor.hex to the motor.\n");
	printf(" 6 - ??? \n");
	printf(" 7 - Profit\n");
	printf("\n");
	printf(" The process can be interrupted, the target device will stay in bootload mode until you complete the upload.\n");
}

void app_prepare_bootload(uint8_t start_addr){
	net_error_e err;
	//Temp location for data:
	uint8_t msgdata[128];
	msg_info_t info;

	//Send a boot info:
	bootstate.address = start_addr;

	bool resp_motor = false;
	bool resp_button = false;

	while(1){
		//Read an arbitrary number of bytes
		int n = uart_read_start(uart,60);

		
		//Message?
		bool message_found = false;
		while (messages_infifo(&rxfifo)){
			message_found = true;
			//We expect the same back:
			err = parse_message(uart->rxfifo,msgdata,128,&info);
			if (err != NET_ERR_NONE){
				debprintf(CLR_YELLOW,0,"Invalid message\n");
				usleep(20000);
			}else{
				//What should we do with the message?
				
				bool ack = ((info.cmd & MASK_CMD_ACK) == MASK_CMD_ACK);
				uint8_t cmd = info.cmd & MASK_CMD;
				//Went ok.
				debprintf(CLR_GREEN,0,"Parse OK: addr %u cmd: %02X ACK: %u\n",info.address,cmd,ack);
				
				if ((cmd == CMD_BOOT_INFO) && ack){

					if (info.address == 3){
						bootstate.address = 4;
						debprintf(CLR_CYAN,0,"ACK from BUTTON.\n");
						resp_button = true;
					}else if (info.address == 4){
						bootstate.address = 3;
						debprintf(CLR_CYAN,0,"ACK from MOTOR.\n");
						resp_motor = true;
					}
				}
			}
		}

		if (!message_found){
			printf_clr(CLR_YELLOW,"No message.\n");	
			if (fifo_free(&rxfifo) == 0){
				fifo_clear(&rxfifo);
				printf("Cleared fifo\n");
			}
		}

		debprintf(CLR_CYAN,0,"\nSending message to %i\n",bootstate.address);
		message_append_tofifo(uart->txfifo,NULL,0,CMD_BOOT_INFO,bootstate.address);
		uart_write_start(uart);

		if (resp_motor && resp_button){
			debprintf(CLR_GREEN,0,"\nBoth devices are in bootload state.\n");
			return;
		}
		select_term();
	}
}

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
	}else if (strcmp(input,"loadmotor") == 0){



		//Opening HEX file
		if (hexfile_open(&filefw,"firmware/motor.hex")){
			printf("Opened MOTOR firmware file. Parsing:\n");
			if (hexfile_process(&filefw)){
				bootstate.size = filefw.binary_size;
				bootstate.todo = bootstate.size;
				bootstate.address = 4;
				app_load_firmware(bootstate.address);
			}else{
				printf("Invalid hex file.\n");
				return 0;
			}
		}else{
			printf("Unable to find hex file.\n");
			return;
		}		
		return;
	}else if (strcmp(input,"motorhex") == 0){
		//Opening HEX file
		if (hexfile_open(&filefw,"firmware/motor.hex")){
			printf("Opened MOTOR firmware file. Parsing:\n");
			if (hexfile_process(&filefw)){
				bootstate.size = filefw.binary_size;
				bootstate.todo = bootstate.size;
				//bootstate.address = 4;
				//app_load_firmware(bootstate.address);
			}else{
				printf("Invalid hex file.\n");
				return 0;
			}
		}else{
			printf("Unable to find hex file.\n");
			return;
		}		
		return;
	}else if (strcmp(input,"loadbutton") == 0){
		//Opening HEX file
		if (hexfile_open(&filefw,"button.hex")){
			printf("Opened BUTTON firmware file. Parsing:\n");
			if (hexfile_process(&filefw)){
				bootstate.size = filefw.binary_size;
				bootstate.todo = bootstate.size;
				bootstate.address = 3;
				app_load_firmware(bootstate.address);
			}else{
				printf("Invalid hex file.\n");
				return;
			}
		}else{
			printf("Unable to find hex file.\n");
			return;
		}		
		return;
	}else if (strcmp(input,"start") == 0){
		app_prepare_bootload(3);
		return;
	}else if (strcmp(input,"startmotor") == 0){
		app_prepare_bootload(4);
		return;
	}else if (strcmp(input,"fifo") == 0){
		fifo_print(&rxfifo);
		return;
	}else if (strcmp(input,"fifoclear") == 0){
		fifo_clear(&rxfifo);
		fifo_print(&rxfifo);
		return;
	}else if (strcmp(input,"info3") == 0){
		app_get_info(3);
		return;
	}else if (strcmp(input,"info4") == 0){
		app_get_info(4);
		return;
	}else if (strcmp(input,"check") == 0){
		app_get_check(bootstate.address);
		return;
	}else if (strcmp(input,"open") == 0){
		app_openuart();
		return;
	}
	printf_clr(CLR_YELLOW,"Unknown command.\n");	
}

bool app_openuart(void){
	//Open a uart
	uart_init(uart);
	uart_setdevice(uart,"/dev/ttyS41");
	if (!uart_configure(uart,B19200,false)){
		//Failed to open.
		printf_clr(CLR_YELLOW,"Try a different tty:?\n");
		uint8_t newname[64];
		fgets(newname,64,stdin);
		//Remove \n
		int len = strlen(newname);	
		newname[len-1] = 0;		

		uart_setdevice(uart,newname);
		if (!uart_configure(uart,B19200,false)){
			printf_clr(CLR_RED,"Nope. Please restart with a correct tty.\n");
			
		}
	}

	//Setup everything:
	fifo_init(&txfifo,txbuffer,UART_BUFFER_SIZE);
	fifo_init(&rxfifo,rxbuffer,UART_BUFFER_SIZE);
	//Attach fifo's to uart.
	uart->rxfifo = &rxfifo;
	uart->txfifo = &txfifo;

	//printf("Attatching funcions to stream\n");
	stream_init(stream,uart);
	stream->write_start = &stream_writestart;
	stream->write_stop = &stream_writestop;

	
}



void term_handle_character(termstate_s* term, char* c){
	bool linebreak = false;

	if (term->inpptr >= (INPUT_BUFFER_MAX -1)){
		term->inpptr = INPUT_BUFFER_MAX-1;
	}

	//printf("inpptr = %i\n",term->inpptr );
	//printf("state  = %i\n",term->state );

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
			
			printf("\r\n");
			parse_userinput(term->buffer);
			memset(term->buffer,0,sizeof(term->buffer));
			term->inpptr = 0;
			

			term->inpptr = 0;
			term->buffer[term->inpptr] = 0;
		}else{
			//terminal_line();
			//printf("CH %u\n",*c);
			printf("%c",*c);
			fflush(stdout);
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
				//printf("Handling char\n");
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


void app_write_nextchunk(void){	
	printf("Writing at %8lu in block %u. Remaining: %8lu/%lu\n",bootstate.offset,bootstate.block,bootstate.todo,bootstate.size);
	if (bootstate.todo >= 64){
		bootstate.todo-= 64;
	}else{
		printf("End\n");
		return;
	}

	uint8_t data[68];
	memset(data,0,68);

	

	uint8_t bootdatalen = 64;
	uint8_t datalen = 68;

	memcpy(&data[0],&bootstate.block,2);
	memcpy(&data[2],&bootstate.offset,2);

	memcpy(&data[4],&filefw.data[bootstate.firmwareoffs],64);

	message_append_tofifo(uart->txfifo,data,datalen,CMD_BOOT_WRITE,bootstate.address);
	uart_write_start(uart);

	
	bootstate.offset += 64;
	

	bootstate.firmwareoffs = (bootstate.block*256) + bootstate.offset;
}


//Where the response should go
net_error_e app_handlemessage(uint8_t* data,msg_info_t* info){

	bool ack = ((info->cmd & MASK_CMD_ACK) == MASK_CMD_ACK);
	uint8_t cmd = info->cmd & MASK_CMD;

	if (info->address != bootstate.address){
		debprintf(CLR_RED,0,"Response from wrong address: %u.\n",info->address);
	}

	if (!ack){
		//This could be our own message
		return NET_ERR_NONE;
	}


	switch(cmd){
		case CMD_PING:
			return NET_ERR_NONE;
		break;
		case CMD_BOOT_INFO:{
			//Ack:
			if (ack){
				debprintf(CLR_CYAN,0,"Received BOOT_INFO ACK from address %02X.\n",info->address);

				//Copy it
				int binfosize = 0;
				memcpy((uint8_t*)&hwinfo,&data[binfosize],sizeof(hwinfo)); binfosize+=sizeof(hwinfo);
				memcpy((uint8_t*)&fwinfo,&data[binfosize],sizeof(fwinfo)); binfosize+=sizeof(fwinfo);

				printf("hwinfo.ver       : %hu\n",hwinfo.version);
				printf("hwinfo.bootaddr  : %u\n",hwinfo.boot_address);
				printf("hwinfo.devaddr   : %u\n",hwinfo.dev_address );
				printf("fwinfo.ver       : %hu\n",fwinfo.version );
				printf("fwinfo.crc       : 0x%04X\n",fwinfo.crc );
				printf("fwinfo.size      : %hu bytes\n",fwinfo.size);
				return NET_ERR_NONE;
			}else{
				return NET_ERR_STATE;
			}
		}
			break;
		//Boot request message, sent on startup.
		case CMD_BOOT_REQ:
			if (bootstate.state == BOOT_WAIT_REQ){
				//Respond with a bootstart message.
				debprintf(CLR_CYAN,0,"Received BOOT_REQ from address %02X. Sending BOOT_START.\n",info->address);
				message_append_tofifo(uart->txfifo,NULL,0,CMD_BOOT_START,bootstate.address);
				uart_write_start(uart);
				bootstate.state = BOOT_WAIT_REQ;
				return NET_ERR_NONE;
			}else{
				return NET_ERR_STATE;
			}
			break;
		case CMD_BOOT_START:
			//Ack:
			if (bootstate.state == BOOT_WAIT_REQ){
				debprintf(CLR_CYAN,0,"Received BOOT_START ACK from address %02X. Writing first block.\n",info->address);
				bootstate.state = BOOT_WRITE;

				app_write_nextchunk();


				return NET_ERR_NONE;
			}else{
				return NET_ERR_STATE;
			}
		case CMD_BOOT_WRITE:
			//Ack from device:
			if ((bootstate.state == BOOT_WRITE) && (ack)){
				

				if (bootstate.todo){
					debprintf(CLR_CYAN,0,"Device received block. Writing next at %hu.\n",bootstate.offset);
					if (bootstate.offset >= 256){
						//Verify crc:
						uint16_t crc;
						uint16_t crcx;
						//Get device's CRC
						memcpy(&crc,data,2);

						//Run our own:
						uint32_t offset = bootstate.firmwareoffs - 256;
						printf("fwoffs, offset %lu %lu\n", bootstate.firmwareoffs,offset);
						crcx = crc16_CCIT(&filefw.data[offset],256,0xFFFF);

						if (crc == crcx){
							debprintf(CLR_GREEN,0,"CRC match: %04X.\n",crc);
						}else{
							debprintf(CLR_RED,0,"CRC mismatch: %04X / %04X.\n",crc,crcx);
							return NET_ERR_CRC;
						}

						//Goto next block:
						if (bootstate.block < 400){ //Limit for testing...
							bootstate.block++;					
							bootstate.offset = 0;
						}else{
							return NET_ERR_NONE;
						}
						debprintf(CLR_CYAN,0,"Done. Next block %02X.\n",bootstate.block);
					}
					app_write_nextchunk();
				}else{
					//Done?
					debprintf(CLR_CYAN,0,"Done writing firmware. Sending BOOT_CHECK.\n",info->address);
					uint16_t appsize = filefw.binary_size;					
					message_append_tofifo(uart->txfifo,(uint8_t*)&appsize,2,CMD_BOOT_CHECK,bootstate.address);
					uart_write_start(uart);
					bootstate.state = BOOT_CHECK;
				}
				return NET_ERR_NONE;
			}else{
				return NET_ERR_STATE;
			}
		case CMD_BOOT_CHECK:
			//Ack:
			if (bootstate.state == BOOT_CHECK){
				debprintf(CLR_CYAN,0,"Received BOOT_CHECK ACK from address %02X.\n",info->address);

				//Get Device CRC:
				uint16_t devcrc;
				memcpy(&devcrc,(uint8_t*)&data[0],2);

				printf("Device CRC: %04X\n",devcrc );
				printf("Local CRC : %04X\n",filefw.binary_crc );
				if (devcrc == filefw.binary_crc){
					debprintf(CLR_GREEN,0,"CRC match. Sending BOOT_FINISH.\n");
					//Set state to finished.
					bootstate.state = BOOT_FINISH;
					uint8_t finsishdata[4];
					memcpy((uint8_t*)&finsishdata[0],(uint8_t*)&filefw.binary_crc,2);
					//App size
					uint16_t appsize = filefw.binary_size;
					memcpy((uint8_t*)&finsishdata[2],(uint8_t*)&appsize,2);
					message_append_tofifo(uart->txfifo,finsishdata,4,CMD_BOOT_FINISH,bootstate.address);
					uart_write_start(uart);
				}else{
					debprintf(CLR_RED,0,"CRC mismatch.\n");
					//Set state to finished.
					bootstate.state = BOOT_FINISH;
				}
				return NET_ERR_NONE;
			}else{
				return NET_ERR_STATE;
			}

		default:
			debprintf(CLR_RED,0,"Received unknown command %02X.\n",info->cmd);
			return NET_ERR_CMD_UNKOWN;

	}

	return NET_ERR_UNDEFINED;
}


//Parse a message on stream index's rxfifo.
net_error_e app_parsemessage(fifo_t* fifo){
	
	net_error_e err;
	//Temp location for data:
	uint8_t msgdata[128];
	msg_info_t info;

	//We expect the same back:
	err = parse_message(fifo,msgdata,128,&info);
	if (err != NET_ERR_NONE){
		//Failed.
		debprintf(CLR_YELLOW,0,"Parse Failed: err %04X\n",err);
		return err;
	}else{
		//What should we do with the message?
		uint8_t cmd = (info.cmd & MASK_CMD);
		//Went ok.
		debprintf(CLR_GREEN,0,"Parse OK: addr %u cmd: %u\n",info.address,cmd);
		
		
		//Does a commandhadler exist?
		return app_handlemessage(msgdata,&info);

		
	}
	return NET_ERR_UNDEFINED;

}

#define RANDSRC "/dev/urandom"
int random_bytes(void *dst, size_t n){
	FILE *f = fopen(RANDSRC, "rb");
	
	size_t r = fread(dst, n, 1, f);
	fclose(f);
	if (r < 1) {
		return 0;
	}
	return 1;
}

int main(int argc, char *argv[]){
	
	term_clear();

	//Test
	parse_userinput("open");



	user_input.inpptr = 0;

	app_init_bootstate();

	app_print_info();

	while(1){
		select_term();
	}
	return 0;
}

void app_get_info(uint8_t address){
	//Clear fifo before we start:
	fifo_clear(&rxfifo);

	//Send a bootstart:
	message_append_tofifo(uart->txfifo,NULL,0,CMD_BOOT_INFO,address);
	uart_write_start(uart);

	int tries = 10;

	while(tries--){
		

		//Read an arbitrary number of bytes
		int n = uart_read_start(uart,20);

		//See if that makes up a message:

		//Message?
		if (messages_infifo(&rxfifo)){
			printf_clr(CLR_CANCEL,"New message.\n");

			//Parse and handle:
			net_error_e ret = app_parsemessage(&rxfifo);
			if (ret == NET_ERR_NONE){
				printf_clr(CLR_GREEN,"MSG OK\n");			
			}else{
				debprintf(CLR_RED,0,"Error: %04X.\n",ret);
			}
		}else{
			printf_clr(CLR_YELLOW,"No message.\n");

			//Are we writing?
			if (bootstate.state == BOOT_WRITE){
				//Block done?
				if (bootstate.offset < (256)){
					//Write a new block.
				//	app_write_nextchunk();
				}
			}
			
		}

		select_term();

	}

	printf("Done.\n");
}

void app_get_check(uint8_t address){
	//Send a bootstart:
	message_append_tofifo(uart->txfifo,NULL,0,CMD_BOOT_CHECK,address);
	uart_write_start(uart);

	int tries = 10;

	while(tries--){
		

		//Read an arbitrary number of bytes
		int n = uart_read_start(uart,20);

		//See if that makes up a message:

		//Message?
		if (messages_infifo(&rxfifo)){

			//Parse and handle:
			net_error_e ret = app_parsemessage(&rxfifo);
			if (ret == NET_ERR_NONE){
				printf_clr(CLR_GREEN,"MSG OK\n");			
			}else{
				debprintf(CLR_RED,0,"Error: %04X.\n",ret);
			}
		}else{
			printf_clr(CLR_YELLOW,"No message.\n");

			//Are we writing?
			if (bootstate.state == BOOT_WRITE){
				//Block done?
				if (bootstate.offset < (256)){
					//Write a new block.
				//	app_write_nextchunk();
				}
			}
			
		}

		select_term();

	}

	printf("Done.\n");
}


void app_load_firmware(uint8_t address){
	//Send a bootstart:
	message_append_tofifo(uart->txfifo,NULL,0,CMD_BOOT_START,address);
	uart_write_start(uart);

	uint16_t timeouts = 0;


	while(1){
		

		//Read an arbitrary number of bytes
		int n = uart_read_start(uart,20);

		//See if that makes up a message:

		//Message?
		if (messages_infifo(&rxfifo)){

			//Parse and handle:
			net_error_e ret = app_parsemessage(&rxfifo);
			if (ret == NET_ERR_NONE){
				printf_clr(CLR_GREEN,"MSG OK\n");	
				timeouts  = 0;		
			}else{
				debprintf(CLR_RED,0,"Error: %04X.\n",ret);
			}
		}else{
			printf_clr(CLR_YELLOW,"No message.\n");
			timeouts++;

			if (timeouts > 20){
				printf("Timeout writing firmware.\n");
				return;
			}
		}

		if (bootstate.state == BOOT_FINISH){
			printf("Done loading file.\n");
			app_init_bootstate();
			return;
		}

		select_term();

	}
}
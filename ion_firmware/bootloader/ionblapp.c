
/*
	Created: 2014-05-14T21:24:47Z
	Author: D. Prins			
		Infant - infant.tweakblogs.net
		mail: prinsje2004 at gmail

		File: inblapp.c
	Bootloader for Sparta ION replacement hardware. 

	This code is released under GNU GPL v3:

    	This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
	
	    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

	    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#define F_CPU 32000000UL
#include <util/delay.h>

#include <avr/io.h>
#include <avr/pgmspace.h>

#include "uarthw.h"
#include "../../lib/network/message.h"

#include "app.h"
#include "sp_driver.h"
#include "flash_wrapper.h"
#include "../fwcommon/eeprom.h"

//Hardware and application info, read from eeprom.
hwinfo_t hwinfo;
fwinfo_t fwinfo;

//Bootload state:
#define BOOT_WAIT_REQ	0
#define BOOT_WAIT_ACK	1
#define BOOT_WRITE		2
#define BOOT_CHECK		3
#define BOOT_FINISH		4
#define BOOT_WAIT_WRITE 5

struct {
	uint8_t state;		//State we are in.
	uint8_t chunk;		//Chunk of block.
	uint16_t block;		//Block we are working on.
	uint16_t crc;		//CRC of current block.	
}bootstate;

//Buffer for a flash page in RAM. (Flash is actually 256 bytes.)
uint8_t pagebuffer[512];

//Local functions:
void clock_switch32M(void);
void bootloader_init(void);
uint16_t read_app_crc(uint16_t);
void app_execute_firmware(void);

void clock_switch32M(void){
	/*Setup clock to run from Internal 32MHz oscillator.*/
	OSC.CTRL |= OSC_RC32MEN_bm;
	while (!(OSC.STATUS & OSC_RC32MRDY_bm));
	CCP = CCP_IOREG_gc;
	CLK.CTRL = CLK_SCLKSEL_RC32M_gc;
	
	OSC.CTRL &= (~OSC_RC2MEN_bm);							// disable RC2M
	PORTCFG.CLKEVOUT = (PORTCFG.CLKEVOUT & (~PORTCFG_CLKOUT_gm)) | PORTCFG_CLKOUT_OFF_gc;	// disable peripheral clock
}

void bootloader_init(void){	
	//Init IO first:	
	PORTC.DIRSET = PIN7_bm | PIN6_bm;
	
	clock_switch32M();
	
	//Set the interrupts, enable low-prio interrupts.
	//The IO vector remap bit is protected, allow acces for 4 clock cycles.
	CCP = CCP_IOREG_gc;
	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm| PMIC_HILVLEN_bm | PMIC_RREN_bm | PMIC_IVSEL_bm;
	
	//Init uart hardware
	uarthw_init();
	
	//Init uart:
	uart_init(uartbus);
	
	uartbus->write_start = &uart1_writestart;
	uartbus->write_stop = &uart1_writestop;	
	
	//Attach fifo's to uart.
	uartbus->rxfifo = &rxfifo1;
	uartbus->txfifo = &txfifo1;
	
	//Enable interrupts.
	sei();
}


/*
	Handles bootload messages.
	Since all this crap doesn't fit in the 4 bootload area, this function is stored somewhere at the end of the applcation flash.
*/
net_error_e _PREBOOT_SECTION app_handlemessage(uart_s* uart, uint8_t* data,msg_info_t* info){

	bool ack = ((info->cmd & MASK_CMD_ACK) == MASK_CMD_ACK);
	uint8_t cmd = info->cmd & MASK_CMD;	

	if (info->address != hwinfo.boot_address){
		return NET_ERR_STATE;
	}
	switch(cmd){
		//Boot request message, sent on startup.
		case CMD_BOOT_INFO:{
			if (!ack){
				uint8_t binfosize = 0;
				uint8_t binfo[32];
				
				memcpy(&binfo[binfosize],(uint8_t*)&hwinfo,sizeof(hwinfo)); binfosize+=sizeof(hwinfo);
				memcpy(&binfo[binfosize],(uint8_t*)&fwinfo,sizeof(fwinfo)); binfosize+=sizeof(fwinfo);				
				
				message_append_tofifo(uart->txfifo,binfo,binfosize,CMD_BOOT_INFO|MASK_CMD_ACK,hwinfo.boot_address);
				uart->write_start();
				return NET_ERR_NONE;
			}			
		}
		break;
		case CMD_BOOT_START:
			if (!ack){
				//Respond with an ACK
				message_append_tofifo(uart->txfifo,NULL,0,CMD_BOOT_START|MASK_CMD_ACK,hwinfo.boot_address);
				uart->write_start();
				//Wait for a block:
				bootstate.state = BOOT_WAIT_WRITE;
				return NET_ERR_NONE;
			}else{
				return NET_ERR_STATE;
			}
		break;
		case CMD_BOOT_WRITE:
			if (!ack){
				//Store the data:
				uint16_t offs;
				uint16_t block;
				memcpy(&block,&data[0],2);
				memcpy(&offs,&data[2],2);
				if (offs < 256){
					memcpy(&pagebuffer[offs],&data[4],64); //Chunk size should be 64.
				}else{
					//Not allowed.
					return NET_ERR_MEMORY;
				}				
				if (block > 56){
					//Not allowed.
					return NET_ERR_MEMORY;
				}					
				//Return a check, 
				uint16_t crcx = crc16_CCIT(pagebuffer,256,0xFFFF);
				message_append_tofifo(uart->txfifo,(uint8_t*)&crcx,2,CMD_BOOT_WRITE|MASK_CMD_ACK,hwinfo.boot_address);
				if (offs == (256-64)){					
					//Write block:
					SP_LoadFlashPage(pagebuffer);
					green_led_tgl();
					EraseWriteAppPage(block);
					green_led_tgl();					
				}						
				uart->write_start();				
			}
			break; 
		case CMD_BOOT_CHECK:
		{
			if (!ack){
				//Get size from message
				memcpy(&fwinfo.size,(uint8_t*)&data[0],2);
				uint16_t appcrc = read_app_crc(fwinfo.size);
				message_append_tofifo(uart->txfifo,(uint8_t*)&appcrc,2,CMD_BOOT_CHECK|MASK_CMD_ACK,hwinfo.boot_address);
				uart->write_start();
				//_delay_ms(200);
			}
		}
		break;
									
		case CMD_BOOT_FINISH:
		{
			if (!ack){	
				//Write data to ROM:				
				memcpy(&fwinfo.crc,(uint8_t*)&data[0],2);
				memcpy(&fwinfo.size,(uint8_t*)&data[2],2);
				
				//Store eeprom:
				eemem_write_block(EEMEM_MAGIC_HEADER_FIRMWARE,(uint8_t*)&fwinfo, sizeof(fwinfo), EEBLOCK_FIRMWARE);
			
				//This would cause a reset to the reset vector... (BOOTRESET)
				//CCP = CCP_IOREG_gc;
				//RST.CTRL = RST_SWRST_bm;
			
				//Jump to the application.
				SP_WaitForSPM();
				//Prevent further writes to application memory.
				SP_LockSPM();
				cli();
				CCP = CCP_IOREG_gc;
				PMIC.CTRL &= ~PMIC_IVSEL_bm; //Reset vectors moved to application
				asm("jmp 0x0000");				
			}			
		}
		break;
		default:			
			return NET_ERR_CMD_UNKOWN;

	}
	return NET_ERR_UNDEFINED;
}

//Returns the CRC of the application flash.
uint16_t read_app_crc(uint16_t size){
	uint16_t crc = 0xFFFF;
	uint16_t blocks = size / 256;
	//Read flash block by block, update CRC.
	for (int i = 0;i<blocks;i++){
		ReadFlashPage(pagebuffer,i);
		crc = crc16_CCIT(pagebuffer,256,crc);
	}
	return crc;
}


//Bootloader main.
int main(void){	
	//Uncomment this block to create the HW info block.
	/*
	hwinfo.boot_address = BOOT_ADDRESS;
	hwinfo.dev_address = DEVICE_ADDRESS;
	hwinfo.version = HARDWARE_VER;
	eemem_write_block(EEMEM_MAGIC_HEADER_HARDWARE,(uint8_t*)&hwinfo, sizeof(hwinfo), EEBLOCK_HARDWARE);
	while(1);
	*/

	bootloader_init();
	
	//Clear all values:
	memset(&hwinfo,0,sizeof(hwinfo_t));
	memset(&fwinfo,0,sizeof(fwinfo_t));
	fwinfo.crc = 0xffff;
	/*hwinfo.boot_address = 0;
	hwinfo.dev_address = 0;
	hwinfo.version = 0;
	fwinfo.crc = 0xffff;
	fwinfo.size = 0;
	fwinfo.version = 0;
	*/
		
	//Read EEPROM:
	eemem_read_block(EEMEM_MAGIC_HEADER_HARDWARE,(uint8_t*)&hwinfo, sizeof(hwinfo), EEBLOCK_HARDWARE);
	eemem_read_block(EEMEM_MAGIC_HEADER_FIRMWARE,(uint8_t*)&fwinfo, sizeof(fwinfo), EEBLOCK_FIRMWARE);
	
	//Calculate the CRC of the application currently stored in flash.
	uint16_t crcapp = 0;	
	if (fwinfo.size > 0){
		//Check application
		crcapp = read_app_crc(fwinfo.size);		
	}	
	
	//Init boot state.
	memset(&bootstate,0,sizeof(bootstate));
	bootstate.state = BOOT_WAIT_ACK;
	
	//Timout before starting applcaition
	uint16_t timeout = 30;
	bool bootloading = false;

	//Bootloader:
	while(1){
		_delay_ms(25);
		red_led_tgl();

		if (!bootloading){
			timeout--;
		}
		
		//Temp location for message data:
		net_error_e err;		
		uint8_t msgdata[160];
		msg_info_t info;
		
		//Check for messages:
		if (messages_infifo(&rxfifo1)){
			//Parse and handle:
			err = parse_message(uartbus->rxfifo,msgdata,128,&info);
			if (err == NET_ERR_NONE){
				//Handle it
				if (app_handlemessage(uartbus,msgdata,&info) == NET_ERR_NONE){
					bootloading = true;
					timeout = 100;
				}
			}		
		}else{
			//No message but fifo is flodded with garbage: Clear it.
			if (fifo_free(uartbus->rxfifo) == 0){
				fifo_clear(uartbus->rxfifo);
			}
		}
		
		if ((timeout == 0) && (!bootloading)){
			//No one has started the bootload process:
			if (crcapp == fwinfo.crc){
				//Application in memory is valid.
				SP_WaitForSPM();
				SP_LockSPM();
				cli();

				//Move interrupt vector table to application.
				CCP = CCP_IOREG_gc;
				PMIC.CTRL &= ~PMIC_IVSEL_bm; 
				//Run the application.
				asm("jmp 0x0000");
			}else{
				timeout = 1000;
				//Stay in the bootloader forever.
			}
		}				
	}
}
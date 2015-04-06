/*
	Created: 6/23/2014 5:07:46 PM - Copyright (c) 
	Author: D. Prins			
		Infant - infant.tweakblogs.net
		mail: prinsje2004 at gmail

		File: bowbus.c
	Used to handle one wire bus communication between Motor Controller / Battery Management and Display
	Uses a timeout to detect a frame end, and relies on escaping being done in the UART ISR.

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

#include <string.h> //For memcpy.
#include "bowbus.h"
#include "crc8.h"
//#include "uart.h"

//Bunch of global variables:
motor_state_s motor;
battery_state_s battery;
display_state_s display; 

//Used by timer.
bool wait_for_last_char = false;

#ifdef AVR
//This function will exist somewhere...
void uart0_writestart(void);
#endif

/*
	Set all the stuff.
*/
void bus_init(bowbus_net_s* bus){
	bus->rx_state = NET_STATE_WAITING;
	bus->rx_buff_cnt = 0;

	bus->tx_state = NET_STATE_WAITING;
	bus->tx_buff_cnt = 0;
	
	bus->new_mesage = false;
	
	//Set the motors's state
	motor.speed = 0;	
	motor.mode = 0;
	motor.needs_update = false;
	motor.mode_needs_update = false;		
	motor.needs_init = true;	
	motor.throttle = 0;
	motor.brake = 0;
	
	//Set the battery's state
	battery.online = false;
	battery.soc = 0;
	battery.distance = 0;
	
	//Set the display's state.
	memset(&display,0,sizeof(display_state_s));
	display.light = false;
	display.cruise = false;
	display.poll_cnt = 0;
	display.needs_update = false;	
	display.menu_timeout = 0 ;
	display.road_legal = true;
	
	wait_for_last_char = false;
	display.function_val2 = 9;
	display.function_val3 = 1;
}

/*
	Character timeout. The message in the buffer may now be handled.
*/
void bus_endmessage(bowbus_net_s* bus){
	//Only if we're not already in a waiting state:
	if (bus->rx_state != NET_STATE_WAITING){
		if ((bus->rx_buff_cnt < 128) && (bus->rx_buff_cnt > 0)) {
			memcpy((uint8_t*)bus->msg_buff,(uint8_t*)bus->rx_buff,bus->rx_buff_cnt);
			bus->msg_len = bus->rx_buff_cnt;
			bus->new_mesage = true;
			bus->rx_buff_cnt = 0;
			bus->rx_state =  NET_STATE_WAITING;
		}
	}	
}

/*
	Handle a single character from the UART.
*/
void bus_receive(bowbus_net_s* bus, uint8_t chr){
	//Reset timer:
	wait_for_last_char = true;
	
	//Receive state machine:
	if (bus->rx_state == NET_STATE_WAITING){
		//Wait for 0x10
		if (chr == FRAME_HEADER){
			bus->rx_state =  NET_STATE_ESCAPING;
		}
	}else if (bus->rx_state == NET_STATE_ESCAPING){
		//If the next character is 0x10, it's escaped to 0x10.
		//Else, it was a start character.
		if (chr == FRAME_ESCAPE){
			if (bus->rx_buff_cnt < 128){
				bus->rx_buff[bus->rx_buff_cnt++] = chr;
			}			
			bus->rx_state =  NET_STATE_READING;
		}else{
			//Previous character was a Frame start.
			if (bus->rx_buff_cnt){
				//Set the new message flag				
				if (bus->rx_buff_cnt < 128){					
					memcpy((uint8_t*)bus->msg_buff,(uint8_t*)bus->rx_buff,bus->rx_buff_cnt);
					bus->msg_len = bus->rx_buff_cnt;
					bus->new_mesage = true;
				}else{
					//? 
				}				
			}
			//This one is data.
			bus->rx_buff_cnt = 2;
			bus->rx_buff[0] = FRAME_HEADER;
			bus->rx_buff[1] = chr;			
			bus->rx_state =  NET_STATE_READING;
		}
	}else if (bus->rx_state == NET_STATE_READING){
		//Maybe attempt to parse the message?
		if (chr == FRAME_ESCAPE){
			//This could be a new message, or an escape.
			bus->rx_state = NET_STATE_ESCAPING;
		}else{
			//This one is data.
			if (bus->rx_buff_cnt < 128){
				bus->rx_buff[bus->rx_buff_cnt++] = chr;
			}			
			//Don't change the state.
		}
	}
}

/*
	Attempt to make sense of the data in the rx buffer, if you're a motor.
*/
bool bus_parse_motor(bowbus_net_s* bus,uint8_t* _data,uint16_t len){
	//Do a CRC check:
	uint8_t crc_calc = crc8_bow(_data,len-1);
	uint8_t crc_msg = _data[len-1];
	
	//Match?
	if (crc_calc != crc_msg){
		return false;
	}
	
	//Message was sent from a device address, to another device address. It works when masked with 0xF0.
	//Not sure what the lower 4 bits do yet.
	uint8_t to   = _data[1] & 0xF0;
	uint8_t from = _data[2] & 0xF0;
	
	//We'll call this next bit the command. I honestly have no clue, but we can differentiate messages on it.
	uint8_t cmd;
	if (len > 3){
		cmd = _data[3];
	}else{
		cmd = 0;
	}
	
	//Pretty sure this is the length of the data in the message.
	uint8_t data_len;
	if (len > 3){
		//Packed in from.
		data_len = _data[2] & 0x0F;
	}else{
		data_len = 0;
	}		
	
	//We're a motor, but we can listen to the communication between battery and display.
	if ((from == ADDR_DISPLAY) && (to == ADDR_BATTERY)){		
		//Clear the offline count:
		display.offline_cnt = 0;
		//Declare it online
		display.online = true;
		
		//Handle different commands.
		if ((cmd == 0x22) && (data_len == 0x02)){//ACK to display query.
			//Update button state:
			display.button_state_prev = display.button_state;
			display.button_state = _data[4] & (BUTT_MASK_TOP|BUTT_MASK_FRONT);			
			//Do not handle button presses if you're a motor.
			//bus_display_buttonpress(bus);		
		}else if ((cmd == 0x26) && (data_len == 0x00)){//ACK to a display update message.
			display.needs_update = false;
		}
	}else if ((from == ADDR_BATTERY) && (to == ADDR_MOTOR)){
		if ((cmd == 0x30) && (data_len == 0x04)){
			//Data contains mode, throttle and speed.
			motor.mode = _data[4];
			motor.throttle = _data[5];
			motor.brake = _data[6];
			display.function_val2 = _data[7];
			
			bus_send_battery_ack(bus,cmd);
		}		
	}
	return true;
}


/*
	Attempt to make sense of the data in the rx buffer if you're a battery.
*/
bool bus_parse_battery(bowbus_net_s* bus,uint8_t* _data,uint16_t len){
	//Do a CRC check:
	uint8_t crc_calc = crc8_bow(_data,len-1);
	uint8_t crc_msg = _data[len-1];
	
	//Match?
	if (crc_calc != crc_msg){
		return false;
	}		

	//Message was sent from a device address, to another device address. It works when masked with 0xF0.
	//Not sure what the lower 4 bits do yet.
	uint8_t to   = _data[1] & 0xF0;
	uint8_t from = _data[2] & 0xF0;
	
	//We'll call this next bit the command. I honestly have no clue, but we can differentiate messages on it.
	uint8_t cmd;
	if (len > 3){
		cmd = _data[3];
	}else{
		cmd = 0;
	}
	
	//Pretty sure this is the length of the data in the message.
	uint8_t data_len;
	if (len > 3){
		//Packed in from.
		data_len = _data[2] & 0x0F;
	}else{
		data_len = 0;
	}		
	
	//Today, we're a battery, and update the display.	
	if ((from == ADDR_DISPLAY) && (to == ADDR_BATTERY)){		
		//Clear the offline count:
		display.offline_cnt = 0;
		//Declare it online
		display.online = true;
		
		//Handle different commands.
		if ((cmd == 0x22) && (data_len == 0x02)){//ACK to display query.
			//Update button state:
			display.button_state_prev = display.button_state;
			display.button_state = _data[4] & (BUTT_MASK_TOP|BUTT_MASK_FRONT);			
			//Handle
			bus_display_buttonpress(bus);		
		}else if ((cmd == 0x26) && (data_len == 0x00)){//ACK to a display update message.
			display.needs_update = false;
		}
	}else if ((from == ADDR_MOTOR) && (to == ADDR_BATTERY)){
		//Declare it online
		motor.online = true;
		
		if ((cmd == 0x30) && (data_len == 14)){
			display.speed =  _data[4] * 2; //Convert back
			motor.status =  _data[5];
			motor.current = _data[6]<<8;
			motor.current |= _data[7];
			motor.voltage = _data[8]<<8;
			motor.voltage |= _data[9];
			display.value1 = _data[10]<<8;
			display.value1 |= _data[11];
			display.value2 = _data[12]<<8;
			display.value2 |= _data[13];
			display.value3 = _data[14]<<8;
			display.value3 |= _data[15];
			display.value4 = _data[16]<<8;
			display.value4 |= _data[17];
		}			
	}
	return true;
}


/*	
	Parse all trafic, and do absolutely nothing.
*/
bool bus_parse(bowbus_net_s* bus,uint8_t* _data,uint16_t len){
	//Do a CRC check:
	uint8_t crc_calc = crc8_bow(_data,len-1);
	uint8_t crc_msg = _data[len-1];
	
	//Match?
	if (crc_calc != crc_msg){
		return false;
	}		

	//Message was sent from a device address, to another device address. It works when masked with 0xF0.
	//Not sure what the lower 4 bits do yet.
	uint8_t to   = _data[1] & 0xF0;
	uint8_t from = _data[2] & 0xF0;
	
	//We'll call this next bit the command. I honestly have no clue, but we can differentiate messages on it.
	uint8_t cmd;
	if (len > 3){
		cmd = _data[3];
	}else{
		cmd = 0;
	}
	
	//Pretty sure this is the length of the data in the message.
	uint8_t data_len;
	if (len > 3){
		//Packed in from.
		data_len = _data[2] & 0x0F;
	}else{
		data_len = 0;
	}		
	
	//Today, we're a battery, and update the display.	
	if ((from == ADDR_DISPLAY) && (to == ADDR_BATTERY)){		
		//Clear the offline count:
		display.offline_cnt = 0;
		//Declare it online
		display.online = true;
		
		//Handle different commands.
		if ((cmd == 0x22) && (data_len == 0x02)){//ACK to display query.
			//Update button state:
			display.button_state_prev = display.button_state;
			display.button_state = _data[4] & (BUTT_MASK_TOP|BUTT_MASK_FRONT);			
			//Handle
			bus_display_buttonpress(bus);		
		}else if ((cmd == 0x26) && (data_len == 0x00)){//ACK to a display update message.
			display.needs_update = false;
		}
	}else if ((from == ADDR_MOTOR) && (to == ADDR_BATTERY)){
		if ((cmd == 0x30) && (data_len == 0x06)){
			display.speed =  _data[4];
			motor.status =  _data[5];
			motor.current = _data[6]<<8;
			motor.current |= _data[7];
			motor.voltage = _data[8]<<8;
			motor.voltage |= _data[9];
		}
	}else if ((from == ADDR_MOTOR) && (to == ADDR_BATTERY)){		
		if ((cmd == 0x30) && (data_len == 14)){			
			display.speed =  _data[4] * 2; //Convert back
			motor.status =  _data[5];
			motor.current = _data[6]<<8;
			motor.current |= _data[7];
			motor.voltage = _data[8]<<8;
			motor.voltage |= _data[9];
			display.value1 = _data[10]<<8;
			display.value1 |= _data[11];
			display.value2 = _data[12]<<8;
			display.value2 |= _data[13];
			display.value3 = _data[14]<<8;
			display.value3 |= _data[15];
			display.value4 = _data[16]<<8;
			display.value4 |= _data[17];			
		}
	}

	return true;
}

/*
	Send back data to the battery, depending on the command.
*/
void bus_send_battery_ack(bowbus_net_s* bus,uint8_t cmd){	
	if (cmd == 0x30){
		uint8_t msg[19];
		msg[0] = 0x10;
		msg[1] = ADDR_BATTERY | ADDR_MASK_RESP; //To
		msg[2] = ADDR_MOTOR | 14; //From | data len
		msg[3] = cmd; //Cmd		
		//Data:
		msg[4] = display.speed /2; //Won't fit otherise
		msg[5] = motor.status; //Status
		msg[6] = display.current>>8;
		msg[7] = (uint8_t)display.current;
		msg[8] = display.voltage>>8;
		msg[9] = (uint8_t)display.voltage;

		msg[10] = display.value1>>8;
		msg[11] = (uint8_t)display.value1;

		msg[12] = display.value2>>8;
		msg[13] = (uint8_t)display.value2;

		msg[14] = display.value3>>8;
		msg[15] = (uint8_t)display.value3;

		msg[16] = display.value4>>8;
		msg[17] = (uint8_t)display.value4;

		msg[18] = crc8_bow(msg,18);		
		//Send the message.
		bus_send(bus,msg,19);
	}
}

//Start sending data over the bus. Returns false if busy.
bool bus_send(bowbus_net_s* bus, uint8_t* data, uint8_t len){
	if (bus->tx_state == NET_STATE_WAITING){	
		memcpy((uint8_t*)bus->tx_buff,(uint8_t*)data,len);
		bus->tx_buff_size = len;
		bus->tx_buff_cnt = 0;
		#ifdef AVR
		uart0_writestart();
		#else
		#endif
		return true;
	}else{
		return false;
	}
}

//Returns true if we're in the process of sending a message.
bool bus_isbusy(bowbus_net_s* bus){
	if (bus->tx_state == NET_STATE_WAITING){
		return true;
	}else{
		return false;
	}	
}

//Send a poll command to display, to receive the state off the buttons.
void bus_display_poll(bowbus_net_s* bus){
	uint8_t msg[6];
	msg[0] = 0x10;
	msg[1] = ADDR_DISPLAY | ADDR_MASK_REQ; //To
	msg[2] = ADDR_BATTERY | 1; //From | data len
	msg[3] = 0x22; //Cmd
	//Send an increment each time this message is sent out.
	msg[4] = display.poll_cnt;
	msg[5] = crc8_bow(msg,5);
	//Must increment this for some reason.
	display.poll_cnt++;
	if (display.poll_cnt > 0x0F){
		display.poll_cnt = 0;
	}
	//Sent the message.
	bus_send(bus,msg,6);
}

//Send a poll command to motor, to receive it's state. We'll update it's mode, throttle and brake. 
void bus_motor_poll(bowbus_net_s* bus){
	uint8_t msg[9];
	msg[0] = 0x10;
	msg[1] = ADDR_MOTOR | ADDR_MASK_REQ;
	msg[2] = ADDR_BATTERY | 4; //Data len
	msg[3] = 0x30; //CMD
	msg[4] = motor.mode; //Mode
	msg[5] = motor.throttle; //GAS
	msg[6] = motor.brake; //BRAKE
	msg[7] = display.function_val2; //Speed limit
	msg[8] = crc8_bow(msg,8);	
	//Sent the message.
	bus_send(bus,msg,9);
}

//Send an interval signal from main loop, mainly for buttons.
void bus_tick(bowbus_net_s* bus){
	//Count how long a button is held down.
	if (display.button_state){
		if (display.menu_downcnt < 0xFFFF){
			display.menu_downcnt++;
		}
	}	
	
	//Automatically go back to default menu when no buttons are pressed.
	if (display.menu_downcnt < 100){
		if (display.menu_timeout){
			display.menu_timeout--;
		}else{
			display.func = 0;
		}
	}	
	
	//Useless?
	if (motor.tick_cnt){
		motor.tick_cnt--;
	}else{
		motor.tick_cnt = 0;
	}
}

//Issued when a button has been pressed:
void bus_display_buttonpress(bowbus_net_s* bus){
	if (display.button_waitforrelease == true){
		if (display.button_state == 0){
			display.button_waitforrelease = false;
			return;
		}else{
			return;
		}
	}
	
	//Parse button presses if has changed to released:
	if ((display.button_state_prev != display.button_state)){
		//Update the timeout:
		display.menu_timeout = 200;
		display.menu_downcnt = 0;
		
		//Handle presses
		if (display.button_state_prev == (BUTT_MASK_TOP|BUTT_MASK_FRONT)){
			display.light = !display.light;
			display.button_waitforrelease = true;
		}else if (display.button_state_prev == BUTT_MASK_TOP){
			//If we've now pressed the middle one too:
			if (display.button_state ==  (BUTT_MASK_TOP|BUTT_MASK_FRONT)){
				//Do nothing.

			}else{
				//Cycle through functions
				display.func++;
				display.func %= 6;
				display.function_val4 = 0;					
			}			
		}else if (display.button_state_prev == BUTT_MASK_FRONT){
			//Handle button press depending on menu:
			if (display.func == 0){
				motor.mode++;
				if (motor.mode == 6){
					motor.mode = 0;
				}
				motor.needs_update = true;
			}else if (display.func == 1){
				display.function_val1++;		
				if (display.function_val1 == 7){
					display.function_val1 = 0;
				}
				motor.needs_update = true;
			}else if (display.func == 2){
				display.function_val2++;
				if (display.function_val2 == 10){
					display.function_val2 = 0;
				}
				motor.needs_update = true;
			}else if (display.func == 3){
				display.function_val3++;
				if (display.function_val3 == 10){
					display.function_val3 = 0;
				}
				motor.needs_update = true;
			}else if (display.func == 4){
				display.function_val4++;
				if (display.function_val4 == 4){
					display.road_legal = !display.road_legal;
					display.function_val4 = 0;
				}
			}else if (display.func == 5){
				display.function_val5++;
				if (display.function_val5 == 4){
					display.function_val5 = 0;
				}				
			}
		}
	}else{
		//Button state is the same.
		if ((display.button_state == BUTT_MASK_TOP) && (display.menu_downcnt > 200)){
			display.func = 8;
		}
	}
}

/*
	Does what the function name says...
*/
bool bus_display_update(bowbus_net_s* bus){
	//Three local variables for the different items on screen, they might get different data values based on menu.
	int32_t distance = display.distance;
	uint16_t speed = display.speed;
	uint8_t soc = display.throttle;
	
	//Decimal numbers
	uint8_t dec[5] = {0};	
	//Bus message
	uint8_t msg[16];
	
	//Assign distance bar
	if (display.func == 4){
		//Road legal menu
		dec[0] = 0;
		dec[1] = DISP_CHAR_NEGATIVE;
		dec[2] = 9;
		dec[3] = DISP_CHAR_CLEAR;
		dec[4] = display.function_val4;
	}else if (display.func == 5){
		//Bottom bar menu
		dec[0] = 0;
		dec[1] = DISP_CHAR_NEGATIVE;
		dec[2] = 3;
		dec[3] = DISP_CHAR_CLEAR;
		dec[4] = display.function_val5;
	}else if (display.func == 1){
		//Display Menu
		dec[0] = 0;
		dec[1] = DISP_CHAR_NEGATIVE;
		dec[2] = 6;
		dec[3] = DISP_CHAR_CLEAR;
		dec[4] = display.function_val1;
	}else if (display.func == 2){
		//MAX PWM menu
		dec[0] = 0;
		dec[1] = DISP_CHAR_NEGATIVE;
		dec[2] = 9;
		dec[3] = DISP_CHAR_CLEAR;
		dec[4] = display.function_val2;
	}else if (display.func == 3){
		//Strain calibration
		uint32_t t = display.strain_th;
		dec[0] = DISP_CHAR_CLEAR;
		//Remove the leading zeros
		if (t < 1000){
			dec[1] = DISP_CHAR_CLEAR;
		}else{
			dec[1] = t / 1000;
			t-= dec[1]*1000;
		}
		if ((t < 100)&& (display.strain_th < 100)){
			dec[2] = DISP_CHAR_CLEAR;
		}else{
			dec[2] = t / 100;
			t-= dec[2]*100;
		}
		if ((t < 10)&& (display.strain_th < 10)){
			dec[3] = DISP_CHAR_CLEAR;
		}else{
			dec[3] = t / 10;
			t-= dec[3]*10;
		}
		dec[4] = t;
	}else if (display.func == 0){
		uint32_t t;
		//Default: show the current.
		if (display.error == 0){			
			distance = display.current;				
			//Display a sign if below zero.
			if (distance < 0){
				//Create the characters.
				//Convert to unsigned.
				t = (-1 * distance);
				//Set the sign
				dec[0] = DISP_CHAR_NEGATIVE;
			}else{
				//Convert to unsigned.
				t = distance;
				//Leave first digit empty.
				dec[0] = DISP_CHAR_CLEAR;
			}
			
			//if (display.function_val5 == 0){
				//Remove the leading zero.
				
				if (t < 1000){
					dec[1] = DISP_CHAR_CLEAR;
				}else{
					dec[1] = t / 1000;
					t-= dec[1]*1000;
				}
				dec[2] = t / 100;
				t-= dec[2]*100;
				dec[3] = t / 10;
				t-= dec[3]*10;
			
			/*}else{
				//Remove the leading zeros
				if (t < 1000){
					dec[1] = DISP_CHAR_CLEAR;
				}else{
					dec[1] = t / 1000;
					t-= dec[1]*1000;
				}
				if (t < 100){
					dec[2] = DISP_CHAR_CLEAR;
				}else{
					dec[2] = t / 100;
					t-= dec[2]*100;
				}
				if (t < 10){
					dec[3] = DISP_CHAR_CLEAR;
				}else{
					dec[3] = t / 10;
					t-= dec[3]*10;
				}
			}*/			
		}else{
			distance = display.error;	
			t = distance;		
			dec[0] = DISP_CHAR_E_UP;
			dec[1] = 0;
		}	

		
		dec[4] = t;
		
		if (display.function_val1 == 1){
			//Display voltage in speedo:
			speed = display.voltage / 10;
		}else if (display.function_val1 == 2){
			speed = display.value1;
		}else if (display.function_val1 == 3){
			speed = display.value2;
		}else if (display.function_val1 == 4){
			speed = display.value3;
		}else if (display.function_val1 == 5){
			speed = display.value4;
		}else if (display.function_val1 == 6){
			//Display power
			speed = display.power / 10;
		
		}else{
			//Just display speed.
			speed = display.speed;
			//speed= display.menu_downcnt;
		}
	}else{
		dec[0] = DISP_CHAR_CLEAR;
		dec[1] = DISP_CHAR_CLEAR;
		dec[2] = DISP_CHAR_CLEAR;
		dec[3] = DISP_CHAR_CLEAR;
		dec[4] = DISP_CHAR_CLEAR;
	}	
	
	//Store them: | 0xC if you want empty instead of 0.
	msg[10] = 0xf0 | (dec[0]);
	msg[11] = ((0x10 * dec[1])) | dec[2];
	msg[12] = ((0x10 * dec[3])) | dec[4];	
	
	//Break the Speed into powers of 10.
	dec[0] = (speed / 100);
	dec[1] = (speed - (100*dec[0])) / 10;
	dec[2] = speed % 10;	
	
	uint8_t data_len = 0x09;
	uint8_t cmd = 0x26;	
	
	msg[0] = FRAME_HEADER;
	msg[1] = ADDR_DISPLAY | 0x01;		//Dunno what the one's for.
	msg[2] = ADDR_BATTERY | data_len;	//I guess that goes there.
	msg[3] = cmd;
	
	//Assign some of the LCD segment masks
	uint8_t dmask = 0;	
	if (motor.mode){
		if (motor.mode & 0x01){
			dmask |= DISP_MASK_LOW;
		}
		if (motor.mode & 0x02){
			dmask |= DISP_MASK_MED;
		}
		if (motor.mode & 0x04){
			dmask |= DISP_MASK_HIGH;
		}
	}else{
		dmask |= DISP_MASK_OFF;
	}	
	msg[4] = dmask;
	
	//Assign some more
	dmask = 0;
	if (!display.road_legal){
		dmask |= DISP_MASK_TRIP;
	}
	if (display.cruise){
		dmask |= DISP_MASK_TOTAL;
	}	
	if (display.light){
		dmask |= DISP_MASK_LIGHT;
	}
	msg[5] = dmask;	
	
	//This guy allows you to set comma's, hide/show the km label and hide/show the soc
	msg[6] = DISP_MASK_SOC;
	if ((display.func == 0) && (display.error == 0)/* && (display.function_val5 == 0)*/){
		msg[6] |= DISP_MASK_COMMA;
	}
		
	//Set the SOC. It has a range 0 to 100, and will ignore a higher value.	
	msg[7] = soc;
	
	//Put the speed in the data, or the menu:
	if (display.func != 0){
		msg[8] = 0x0F;
		msg[9] = 0x0C | (display.func<<4);
	}else{
		msg[8] = 0xC0 | 0x01 * dec[0];
		msg[9] = ((0x10 * dec[1])) | dec[2];
	}
	
	//Computer the CRC:
	uint8_t crc = crc8_bow(msg,13); 
	msg[13] = crc;
	
	//Send it out.
	return bus_send(bus,msg,14);	
}

//Message should be auto escaped when sending.
//void bus_make_message(uint8_t to, uint8_t from, uint8_t cmd, uint8_t* data,uint8_t data_len, uint8_t* msg_out){}
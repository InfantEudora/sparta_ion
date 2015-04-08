/*
	Created: 6/23/2014 5:07:46 PM - Copyright (c) 
	Author: D. Prins			
		Infant - infant.tweakblogs.net
		mail: prinsje2004 at gmail

		File: bowbus.c
	Used to handle one wire bus communication between Motor Controller / Battery Management and Display
	Uses a timeout to detect a frame end, and relies on escaping being done in the uart ISR.

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

#ifndef BOWBUS_H_
#define BOWBUS_H_

#include <stdbool.h>
#include <stdint.h>

//States for the network FSM:
#define NET_STATE_WAITING	0
#define NET_STATE_ESCAPING	1
#define NET_STATE_READING	2
#define NET_STATE_WRITING	3

//All the frames this network seems to need:
#define FRAME_HEADER		0x10
#define FRAME_ESCAPE		0x10

//Device address constants:
#define ADDR_MOTOR			0x00
#define ADDR_BATTERY		0x20
#define ADDR_0X40			0x40 //<--?
#define ADDR_DISPLAY		0xC0

#define ADDR_MASK_REQ		0x01
#define ADDR_MASK_RESP		0x02

//Fixed buffer size
#define RX_BUFF_MAX			130
#define TX_BUFF_MAX			130

//Mask for buttons in button message.
#define BUTT_MASK_TOP		0x01
#define BUTT_MASK_FRONT		0x02

//Segments on the display, data[4]
#define DISP_MASK_OFF		0x01|0x02 //Both or'd values have the same effect.
#define DISP_MASK_LOW		0x04|0x08
#define DISP_MASK_MED		0x10|0x20
#define DISP_MASK_HIGH		0x40|0x80

//Segments on the display, data[5]
#define DISP_MASK_TOOL		0x01|0x02 //Both or'd values have the same effect.
#define DISP_MASK_TRIP		0x04|0x08
#define DISP_MASK_TOTAL		0x10|0x20
#define DISP_MASK_LIGHT		0x40|0x80

//Enable segments, data[6]
#define DISP_MASK_SOC		0x01
#define DISP_MASK_COMMA		0x10|0x20 //Both or'd values have the same effect.
#define DISP_MASK_KM		0x40|0x80

//In the ditance bar,
#define DISP_CHAR_NEGATIVE	10	//'-' character in distance
#define DISP_CHAR_B_LOW		11	//'b' Lower case b character
#define DISP_CHAR_CLEAR		12	//' ' Space, empty character. 
#define DISP_CHAR_D_LOW		13	//'d' Lower case d.
#define DISP_CHAR_E_UP		14	//'E' Upper case E
#define DISP_CHAR_F_UP		15	//'F' Upper case F


//Typedef them, although they're all used once and declared extern.
typedef struct bowbus_net_s bowbus_net_s;
typedef struct motor_state_s motor_state_s;
typedef struct battery_state_s battery_state_s;
typedef struct display_state_s display_state_s;

struct bowbus_net_s{
	uint16_t rx_buff_cnt;
	uint8_t  rx_buff[RX_BUFF_MAX];	
	uint8_t  rx_state;
	
	uint16_t tx_buff_size; //Size of data
	uint16_t tx_buff_cnt; //Send 'pointer'. 
	uint8_t  tx_buff[TX_BUFF_MAX];
	uint8_t  tx_state;
	
	bool	message_end;
	
	bool	 new_mesage;
	uint8_t  msg_buff[TX_BUFF_MAX];	
	uint8_t  msg_len;
};

/*
	Motor variables.
*/
struct motor_state_s{
	uint16_t speed;			//The speed times 10.
	uint16_t tick_cnt;			//Some weird counter.
	uint8_t mode;			//7 power modes.
	uint8_t throttle;		//Throttle 0-100
	uint8_t brake;			//Brake power 0-100
	uint8_t status;			//Motor status flags.
	uint16_t voltage;		//Voltage measured by motor. 100 = 10.0 Volt
	int16_t current;		//Current measured by motor. 100 = 1.00 Amp
	uint16_t offline_cnt;	//If too high, the motor is offline.
	bool online;
	bool needs_init;
	bool needs_update;
	bool mode_needs_update;
	
};

struct battery_state_s{	
	uint32_t distance;	//Or put it in the motor...
	bool online;
	uint8_t soc;
};


//Selectable in the F1 menu:
#define NUM_SPD_SPEED	0	//Display speed in speedo.
#define NUM_SPD_VOLTAGE	1	//Display voltage in speedo

//The speed modes scale the throttle power.

//F3 has the regen power.

struct display_state_s{
	//State the buttons are in
	uint8_t button_state_prev;
	uint8_t button_state;
	uint8_t button_waitforrelease;
	
	//Settings
	uint8_t function_val1;	//Values for the function menu: Speed/Voltage
	uint8_t function_val2;	//Values for the function menu: Brake Strength
	uint8_t function_val3;	//Values for the function menu: Brake PWM Test
	uint8_t function_val4;	//Values for the function menu: Activates it.
	uint8_t function_val5;	//Values for the function menu: ?
	
	//Menu timeout:
	uint16_t menu_timeout;
	
	
	uint16_t menu_downcnt;	//Hold down counter
	
	uint8_t poll_cnt;	//Counter which seems to be required for polling.
	bool light;
	bool cruise;			
	bool online;			//If the display is responding.
	uint16_t offline_cnt;	//If too high, the display is offline.
	
	//Custom stuff.
	uint16_t voltage;
	int16_t power;
	int16_t current;
	int16_t strain;			//Strain value
	int16_t strain_th;		//Strain threshold.
	
	uint8_t func;
	
	bool road_legal;	//When true, middle button overrides throttle.	
	
	uint32_t distance;	//What value should be in the distance bar.
	uint16_t speed;		//Value in the speed bar. 10 km/h is stored as 100
	uint8_t soc;		//What value should be in the soc bar
	uint8_t throttle;	//Throttle value in percent.
	uint32_t error;		//Error value that it displayed when not 0.

	//Bunch of test values.
	uint16_t value1;		
	uint16_t value2;
	uint16_t value3;
	uint16_t value4;
	
	bool needs_update;
};

void bus_init(bowbus_net_s* bus);
void bus_endmessage(bowbus_net_s* bus);
void bus_receive(bowbus_net_s* bus, uint8_t chr);
bool bus_parse(bowbus_net_s* bus,uint8_t* _data,uint16_t len);
bool bus_parse_motor(bowbus_net_s* bus,uint8_t* _data,uint16_t len);
bool bus_parse_battery(bowbus_net_s* bus,uint8_t* _data,uint16_t len);
bool bus_send(bowbus_net_s* bus,uint8_t* data, uint8_t len);
bool bus_isbusy(bowbus_net_s* bus);

void bus_tick(bowbus_net_s* bus);

void bus_send_battery_ack(bowbus_net_s* bus,uint8_t cmd);

void bus_display_poll(bowbus_net_s* bus);
void bus_display_tick(bowbus_net_s* bus);
void bus_display_buttonpress(bowbus_net_s* bus);
bool bus_display_update(bowbus_net_s* bus);
void bus_display_clear_error(bowbus_net_s* bus);

void bus_motor_poll(bowbus_net_s* bus);


//We have one motor.
extern motor_state_s motor;

//We have one main battery.
extern battery_state_s battery;

//We have one display.
extern display_state_s display;

//Wait for last character?
extern bool wait_for_last_char;

#endif /* BOWBUS_H_ */
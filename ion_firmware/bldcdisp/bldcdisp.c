/*
	Created: 2014-06-23T17:07:46Z
	Author: D. Prins			
		Infant - infant.tweakblogs.net
		mail: prinsje2004 at gmail

		File: bldcdisp.c
	Application running on Sparta ION Control Board. 	
	Responsible for generating motor drive signals and display communication.
	Features and hardware definitions are set/defined in app.h

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
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <avr/interrupt.h>
#include "app.h"
#include "adc.h"
#include "uart.h"
#include "../fwcommon/eeprom.h"
#include "../../lib_ion/bowbus.h"


#define REF_1V0				ADC_REFSEL_INT1V_gc
//Some version of AVR studio may complain here, and wants this set to ADC_REFSEL_VCC_gc
#define REF_3V3				ADC_REFSEL_INTVCC_gc 

//Absolute PWM limits.
#define PWM_MAX				420
#define PWM_SET_MAX			PWM_MAX - 15
#define PWM_MIN				35
#define PWM_SET_MIN			PWM_MIN //PWM_MIN+3

//Dynamic PWM limits.
volatile uint16_t pwm_set_min = PWM_SET_MIN;
volatile uint16_t pwm_set_max = PWM_SET_MAX;

bool flagsave = false;			//Set when settings need to be saved
settings_t eepsettings;			//The settings.

bool usehall = true;
bool use_throttle = true;		//Apply throttle.
bool use_pedalassist = true;		//Apply throttle.

bool use_brake = false;			//Apply brake
bool flag_keep_pwm = false;

bool flag_brake_lowrpm = false;	//Brake has turned lower mosfets on.

volatile uint8_t brake				= 0;	//0 = no regen, 100 = max regen.
volatile uint8_t throttle_prev		= 0;	
volatile uint8_t throttle			= 0;		
volatile uint8_t throttle_override  = 0;	//Override for testing.
volatile uint8_t pedal_signal_prev	= 0;		
volatile uint8_t pedal_signal		= 0;		
volatile uint8_t brake_level		= 0;	//Brake power

//Bitfields for status flags
#define STAT_OK					0x0000
#define STAT_THROTTLE_FAULT		0x0001
#define STAT_OVER_VOLTAGE		0x0002
#define STAT_UNDER_VOLTAGE		0x0004
#define STAT_REGEN_HIGH			0x0008
#define STAT_STOPPED			0x0010
#define STAT_USETHROTTLE		0x0020
#define STAT_HALL_FAULT			0x0040
#define STAT_BRAKE_FAULT		0x0080

//All the possible errors 
#define STAT_ERROR_MASK			(STAT_THROTTLE_FAULT | STAT_BRAKE_FAULT | STAT_OVER_VOLTAGE | STAT_UNDER_VOLTAGE | STAT_REGEN_HIGH | STAT_STOPPED )

//Status bitmap
uint32_t status = 0;

//Functions:
void status_set(uint32_t);
void status_clear(void);
bool isfaultset(void);
void pwm_less(uint8_t by);
void pwm_more(uint8_t by);
void commute_start(void);
void commute_forward(void);
void commute_backward(void);

bool get_throttle(void);
bool get_brake(void);

//For Bowbus
bool bus_check_for_message(void);

uint16_t cnt_2 = 0;
uint8_t led_off_cnt = 0;
uint8_t flag_timer = 0;

#define HALL_MASK (PIN4_bm | PIN5_bm | PIN6_bm)

//Hall peaks
uint16_t hallhi = 0;
uint16_t halllo = 0;
	
uint8_t hall_state = 0;
uint8_t hall_prev = 0;
uint8_t flag_hall_changed = 0;

uint16_t pwm = PWM_SET_MAX;
uint16_t pwm_prev = PWM_SET_MAX;

uint8_t off = 0;	//Freewheel on/off.
uint8_t tie = 0;	//Tie low to GND.

uint8_t startup = 10;

//If you want the wheel to go the other way.
#define FORWARD 0
#define BACKWARD 1
uint8_t direction = BACKWARD;		//This is actually forward....


uint16_t strain_threshhold = 200;	//Calibration value for strain sensor.
uint8_t strain_cal_inc = 1;			
int32_t ad_strain = 0;
int32_t ad_strain_sum = 0;
int32_t ad_strain_av = 0;
uint16_t strain_cnt = 0;			//If the strain registered, how long it should be on for.

uint32_t ad_temp_ref = 0;			//Factory reference.
uint32_t ad_temp = 0;
uint32_t ad_temp_sum = 0;
uint32_t ad_temp_av = 0;

uint32_t ad_throttle = 0;
uint32_t ad_brake = 0;

uint32_t ad_voltage = 0;
uint32_t ad_voltage_sum = 0;
uint32_t ad_voltage_av = 0;

int32_t ad_current = 0;
int32_t ad_current_sum = 0;		//Sum of current over meas_cnt time.
int32_t ad_current_av = 0;		//The final average, computed on display update.

int32_t power_av = 0;

uint16_t ad_current_regcnt = 0;
int32_t ad_current_regsum = 0;		//Average computed per commutation
int32_t ad_current_regav = 0;		//The average for the commutation loop.

uint16_t meas_cnt = 0;

int32_t ad_currentmin = 0;
int32_t ad_currentmax = 0;

//Get all analog samples on this board.
uint8_t ad_max_samples = 8;


uint16_t time_prev = 0;
uint16_t time_now = 0;
uint32_t time_comm = 0;

uint32_t time_comm_av = 0;
uint32_t time_comm_av_last = 0;

uint32_t commtime_sum = 0;	//Sum of commutation times.
uint32_t commtime_cnt = 0;	//Number of commutations within that period
uint32_t speed = 0;			//in kmh * 1000. So 10000 = 10.0 kmh

uint16_t cnt_commutations = 0;
uint16_t cnt_rotations = 0;

//Regulaton settings
uint8_t slope_throttle = 1;	//Rate at which PWM may increase for throttle.
uint8_t slope_brake = 10;		//Rate at which PWM may increase for brake.

//Throttle limits.
#define THROTTLE_DISC	100		//Throttle disconnected. //600 safe value
#define THROTTLE_LOW	730		//Lowest
#define THROTTLE_HIGH	2890	//Highest
#define THROTTLE_OVER	3900	//Too high //3100 safe value

uint16_t throttle_tick = 0;
bool throttle_cruise = false;

//Strain min and maximum recorded value.
int16_t highestforce = 0;
int16_t lowestforce = 0;

bowbus_net_s bus;

void init(void){	
	PORTD.DIRSET = PIN0_bm;
	PORTC.DIRSET = PIN3_bm | PIN2_bm | PIN1_bm | PIN0_bm;	
}

void clock_switch32M(void){
	/*Setup clock to run from Internal 32MHz oscillator.*/
	OSC.CTRL |= OSC_RC32MEN_bm;
	while (!(OSC.STATUS & OSC_RC32MRDY_bm));
	CCP = CCP_IOREG_gc;
	CLK.CTRL = CLK_SCLKSEL_RC32M_gc;	
	OSC.CTRL &= (~OSC_RC2MEN_bm);
	PORTCFG.CLKEVOUT = (PORTCFG.CLKEVOUT & (~PORTCFG_CLKOUT_gm)) | PORTCFG_CLKOUT_OFF_gc;	// disable peripheral clock	
}

void init_pwm(void){
	//PIN direction:
	PORTC.DIRSET = 0xFF;
	//Setup PWM
	TCC0.CTRLA = TC_CLKSEL_DIV8_gc;
	TCC0.CTRLB = TC_WGMODE_SINGLESLOPE_gc;
	TCC0.PER = PWM_MAX;
	//Default PWM value
	TCC0.CCA = pwm_set_max;
	TCC0.CCB = pwm_set_max;
	TCC0.CCC = pwm_set_max;	

	//HiRes extensions
	HIRESC.CTRLA = HIRES_HREN_TC0_gc;
	
	//Dead time:
#if(HARDWARE_VER == HW_CTRL_REV1)
	//Sparta ION Board has hardware deadtime.
	AWEXC.DTHS = 0;
	AWEXC.DTLS = 0;
#else
	AWEXC.DTHS = 1;
	AWEXC.DTLS = 1;
#endif
	
	AWEXC.STATUS = AWEX_DTHSBUFV_bm | AWEX_DTLSBUFV_bm | AWEX_FDF_bm;
	
#if(HARDWARE_VER == HW_CTRL_REV1)
	//This hardware has inverted high side inputs, so we'll invert them again.	
	PORTC.PIN0CTRL = PORT_INVEN_bm;
	PORTC.PIN2CTRL = PORT_INVEN_bm;
	PORTC.PIN4CTRL = PORT_INVEN_bm;
	
	//The Low/High are also switched with respect to first board, it routed nicer.
#endif
}

#if(HARDWARE_VER == HW_CTRL_REV1)

void sector_0(void){
	AWEXC.OUTOVEN =  PIN0_bm | PIN1_bm;
	AWEXC.CTRL =  AWEX_DTICCAEN_bm;	
	PORTC.OUTCLR = PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm |  PIN4_bm;
	PORTC.OUTSET = PIN5_bm;
}

void sector_1(void){
	AWEXC.OUTOVEN = PIN0_bm |  PIN1_bm;
	AWEXC.CTRL =  AWEX_DTICCAEN_bm;	
	PORTC.OUTCLR = PIN0_bm | PIN1_bm |  PIN2_bm | PIN4_bm | PIN5_bm;
	PORTC.OUTSET = PIN3_bm;
}

void sector_2(void){
	AWEXC.OUTOVEN =  PIN4_bm| PIN5_bm;
	AWEXC.CTRL =  AWEX_DTICCCEN_bm;	
	PORTC.OUTCLR = PIN0_bm | PIN1_bm |  PIN2_bm | PIN4_bm | PIN5_bm;
	PORTC.OUTSET = PIN3_bm;
}

void sector_3(void){
	AWEXC.OUTOVEN =  PIN4_bm| PIN5_bm;
	AWEXC.CTRL =  AWEX_DTICCCEN_bm;	
	PORTC.OUTCLR =  PIN0_bm | PIN2_bm | PIN3_bm | PIN4_bm | PIN5_bm;
	PORTC.OUTSET = PIN1_bm;
}

void sector_4(void){
	AWEXC.OUTOVEN =  PIN2_bm | PIN3_bm;
	AWEXC.CTRL =  AWEX_DTICCBEN_bm;	
	PORTC.OUTCLR = PIN0_bm | PIN2_bm | PIN3_bm | PIN4_bm | PIN5_bm;
	PORTC.OUTSET = PIN1_bm;
}

void sector_5(void){
	AWEXC.OUTOVEN =  PIN2_bm | PIN3_bm;
	AWEXC.CTRL =  AWEX_DTICCBEN_bm;	
	PORTC.OUTCLR = PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm |  PIN4_bm;
	PORTC.OUTSET = PIN5_bm;
}
#else
void sector_0(void){
	AWEXC.OUTOVEN =  PIN0_bm | PIN1_bm;
	AWEXC.CTRL =  AWEX_DTICCAEN_bm;	
	PORTC.OUTCLR = PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm |  PIN5_bm;
	PORTC.OUTSET = PIN4_bm;
}

void sector_1(void){
	AWEXC.OUTOVEN = PIN0_bm |  PIN1_bm;
	AWEXC.CTRL =  AWEX_DTICCAEN_bm;	
	PORTC.OUTCLR = PIN0_bm | PIN1_bm |  PIN3_bm | PIN4_bm | PIN5_bm;
	PORTC.OUTSET = PIN2_bm;
}

void sector_2(void){
	AWEXC.OUTOVEN =  PIN4_bm| PIN5_bm;
	AWEXC.CTRL =  AWEX_DTICCCEN_bm;	
	PORTC.OUTCLR = PIN0_bm | PIN1_bm |  PIN3_bm | PIN4_bm | PIN5_bm;
	PORTC.OUTSET = PIN2_bm;
}

void sector_3(void){
	AWEXC.OUTOVEN =  PIN4_bm| PIN5_bm;
	AWEXC.CTRL =  AWEX_DTICCCEN_bm ;	
	PORTC.OUTCLR =  PIN1_bm | PIN2_bm | PIN3_bm | PIN4_bm | PIN5_bm;
	PORTC.OUTSET = PIN0_bm;
}

void sector_4(void){
	AWEXC.OUTOVEN =  PIN2_bm | PIN3_bm;
	AWEXC.CTRL =  AWEX_DTICCBEN_bm;	
	PORTC.OUTCLR = PIN1_bm | PIN2_bm | PIN3_bm | PIN4_bm | PIN5_bm;
	PORTC.OUTSET = PIN0_bm;
}

void sector_5(void){
	AWEXC.OUTOVEN =  PIN2_bm | PIN3_bm;
	AWEXC.CTRL =  AWEX_DTICCBEN_bm;	
	PORTC.OUTCLR = PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm |  PIN5_bm;
	PORTC.OUTSET = PIN4_bm;
}
#endif

void commute_start(void){
	//Set deadtime, HW specific
#if(HARDWARE_VER == HW_CTRL_REV1)	
	AWEXC.DTHS = 0;
	AWEXC.DTLS = 0;
#else
	AWEXC.DTHS = 1;
	AWEXC.DTLS = 1;
#endif
	//Apply.
	AWEXC.STATUS = AWEX_DTHSBUFV_bm | AWEX_DTLSBUFV_bm | AWEX_FDF_bm;
	
	//Reset highside refresh thingy:
	TCD0.CNT = 0;
}

void switch_sector(uint8_t sect){
	switch(sect){
		case 0:
			sector_0();
			break;
		case 1:
			sector_1();
			break;
		case 2:
			sector_2();
			break;
		case 3:
			sector_3();
			break;
		case 4:
			sector_4();
			break;
		case 5:
			sector_5();
			break;
	}
}

void pwm_freewheel(void){
	PORTC.OUTCLR = PIN5_bm | PIN4_bm | PIN3_bm | PIN2_bm | PIN1_bm | PIN0_bm;	
	//Dead time:
	AWEXC.OUTOVEN = 0;	
	AWEXC.CTRL = 0;
}

/*
	Connect all lower side MOSFETs, it's like shorting the wires together.
	Better not do this while driving really fast.
*/
void tie_ground(void){
	PORTC.OUTCLR = PIN5_bm | PIN4_bm | PIN3_bm | PIN2_bm | PIN1_bm | PIN0_bm;
	//Dead time:
	AWEXC.OUTOVEN = 0;
	AWEXC.CTRL = 0;

	//Enable lower FETs
	#if(HARDWARE_VER == HW_CTRL_REV1)	
		PORTC.OUTSET =  PIN5_bm |  PIN3_bm |  PIN1_bm;
	#else
		PORTC.OUTSET =  PIN4_bm |  PIN2_bm |  PIN0_bm;
	#endif	
}

//Estimate the PWM value when freewheeling which would yield close to no current.
uint16_t estimate_pwm_byspeed(){
	int16_t pwm_est = 0;
#if((HARDWARE_VER == HW_BLDC_REV1)||(HARDWARE_VER == HW_BLDC_REV0))
	if (speed > 0){
		pwm_est = pwm_set_max - ((((pwm_set_max) - (pwm_set_min)) / 30) * (speed/10));
		
		if (pwm_est < pwm_set_min){
			pwm_est = pwm_set_min;
		}
		if (pwm_est > pwm_set_max){
			pwm_est = pwm_set_max;
		}
	}else{
		pwm_est = pwm_set_max;
	}
#else
	//Inverted PWM:
	if (speed > 0){
		pwm_est = pwm_set_min + (((uint16_t)speed*12UL)/10UL);
		
		if (pwm_est < pwm_set_min){
			pwm_est = pwm_set_min;
		}
		if (pwm_est > pwm_set_max){
			pwm_est = pwm_set_max;
		}
	}else{
		pwm_est = pwm_set_min;
	}
#endif
	return (uint16_t)pwm_est;
}					

//Get all analog measurement values.
void get_measurements(void){
	uint32_t usample = 0;
	#if(HARDWARE_VER != HW_CTRL_REV1)
	 uint32_t gsample = 0;
	#else
	 int32_t ssample = 0;
	#endif
	uint32_t bsample = 0;
	int32_t isample = 0;	

	//Enable lower FETs
	#if(HARDWARE_VER == HW_CTRL_REV1)
		//This board does not have brake/throttle, but a strain.
		for (uint16_t i=0;i<ad_max_samples;i++){
			adc_init_single_ended(REF_3V3,ADC_CH_MUXPOS_PIN7_gc); //Bat Voltage.
			usample += adc_getsample();
			adc_init_strain(REF_3V3); //Strain Sensor.
			ssample += adc_getsample();	
			adc_init_temperature();//Internal temp
			bsample += adc_getsample();	
			adc_init_differential(REF_3V3,ADC_CH_MUXPOS_PIN0_gc); //Motor current.		
			isample += (int16_t)adc_getsample();		
		}
	#else
		//This board has analog inputs for brake and throttle.
		for (uint16_t i=0;i<ad_max_samples;i++){
			adc_init_single_ended(REF_3V3,ADC_CH_MUXPOS_PIN7_gc); //Bat Voltage.
			usample += adc_getsample();
			adc_init_single_ended(REF_3V3,ADC_CH_MUXPOS_PIN3_gc); //Throttle
			gsample += adc_getsample();
			adc_init_single_ended(REF_3V3,ADC_CH_MUXPOS_PIN2_gc); //Brake
			bsample += adc_getsample();		
			adc_init_differential(REF_3V3,ADC_CH_MUXPOS_PIN11_gc); //Motor current.		
			isample += (int16_t)adc_getsample();		
		}
	#endif
	
	//Convert 'em back to 12 bits.
	ad_voltage = usample/(uint32_t)ad_max_samples;	
	ad_current = isample/(int32_t)ad_max_samples;

	//Hardware specific inputs.
	#if(HARDWARE_VER == HW_CTRL_REV1)		
		ad_strain = (int32_t)ssample/(int32_t)ad_max_samples;		
		ad_strain_sum += ad_strain;
		
		ad_temp = bsample/ad_max_samples;
		ad_temp_sum += ad_temp;
	#else
		ad_throttle = gsample/ad_max_samples;
		ad_brake = bsample/ad_max_samples;
	#endif
	
	//Summing things that need averaging.
	meas_cnt++;
	ad_voltage_sum += ad_voltage;
	ad_current_sum += ad_current;
	
	//Per commutation current.
	ad_current_regcnt++;
	ad_current_regsum += ad_current;
}


//Returns true if the hall sensors produce a valid signal.
bool get_hall(void){		
	//filter:
	hallhi = 0;
	halllo = 0xFFFF;
	
	//Clear hall
	hall_state = 0;
	
	#if(HARDWARE_VER == HW_CTRL_REV1)
	//Maybe....? Yep... they're open-drain.
	PORTD.DIRCLR = PIN5_bm | PIN6_bm | PIN7_bm;
	
	//Use internal pullups
	PORTD.PIN5CTRL = PORT_OPC_PULLUP_gc;
	PORTD.PIN6CTRL = PORT_OPC_PULLUP_gc;
	PORTD.PIN7CTRL = PORT_OPC_PULLUP_gc;
	
	if  (PORTD.IN & PIN5_bm){
		hall_state |= 0x01;
	}
	if  (PORTD.IN & PIN6_bm){
		hall_state |= 0x04;
	}
	if  (PORTD.IN & PIN7_bm){
		hall_state |= 0x02;
	}
	#else
	//Maybe....? Yep... they're open-drain.
	PORTA.DIRCLR = PIN4_bm | PIN5_bm | PIN6_bm;
	
	//Use internal pullups
	PORTA.PIN4CTRL = PORT_OPC_PULLUP_gc;
	PORTA.PIN5CTRL = PORT_OPC_PULLUP_gc;
	PORTA.PIN6CTRL = PORT_OPC_PULLUP_gc;	
	
	if  (PORTA.IN & PIN4_bm){
		hall_state |= 0x01;
	}
	if  (PORTA.IN & PIN5_bm){
		hall_state |= 0x04;
	}
	if  (PORTA.IN & PIN6_bm){
		hall_state |= 0x02;
	}	
	#endif
	
	//Change?
	if (hall_prev != hall_state){
		flag_hall_changed = 1;
	}
	
	hall_prev = hall_state;	
	
	if ((hall_state == 0)|| (hall_state == 0x07)) {
		return false;
	}else{
		return true;
	}
	return false;	
}



//Hall states in oder: 0x05,0x04,0x06,0x02,0x03,0x01;
//Change FETs in forward direction.
void commute_forward(void){
	//Reset high-side gate-drive counter:
	TCD0.CNT = 0;
	
	switch(hall_state){
		case 0x05:
		switch_sector(3);
		break;
		case 0x04:
		switch_sector(4);
		break;
		case 0x06:
		switch_sector(5);
		break;
		case 0x02:
		switch_sector(0);
		break;
		case 0x03:
		switch_sector(1);
		break;
		case 0x01:
		switch_sector(2);
		break;
	}
}

void commute_backward(void){
	//Reset high-side gate-drive counter:	
	TCD0.CNT = 0;
	
	switch(hall_state){	
		case 0x05:
		switch_sector(0);
		break;
		case 0x04:
		switch_sector(1);
		break;
		case 0x06:
		switch_sector(2);
		break;
		case 0x02:
		switch_sector(3);
		break;
		case 0x03:
		switch_sector(4);
		break;
		case 0x01:
		switch_sector(5);
		break;
	}
}

/*
Fakes the hall position forward
*/
void fake_hall_forward(void){	
	switch(hall_state){
		case 0x05:
		hall_state = 0x04;
		break;
		case 0x04:
		hall_state = 0x06;
		break;
		case 0x06:
		hall_state = 0x02;
		break;
		case 0x02:
		hall_state = 0x03;
		break;
		case 0x03:
		hall_state = 0x01;
		break;
		case 0x01:
		hall_state = 0x05;
		break;
	}
}

/*
	This function should read the throttle, depending on your hardware.
	It can be read from an analog signal, an on/off signal or from a message.
	throttle is set from 0-100
*/
bool get_throttle(void){
	//Remember prev throttle value;
	throttle_prev = throttle;
	
	#if((OPT_THROTTLE==FW_THROTTLE_DIRECT)||(OPT_THROTTLE==FW_THROTTLE_MASTER))
	//Convert analog throttle values.
	if (ad_throttle < THROTTLE_DISC){
		return false;
	}
	if (ad_throttle > THROTTLE_OVER){
		return false;
	}	
	uint32_t t;	
	//This is all fine.
	if (ad_throttle <= THROTTLE_LOW){
		throttle = 0;
	}else if (ad_throttle >= THROTTLE_HIGH){
		throttle = 100;
	}else{
		//In between		
		t = 100000 / (THROTTLE_HIGH-THROTTLE_LOW);
		t *= (ad_throttle - THROTTLE_LOW);
		throttle = t / 1000; 
	}
	#endif
	
	//If we are cruising:
	if (throttle_cruise){
		throttle = 100;
	}		
	
	#if(OPT_THROTTLE==FW_THROTTLE_SLAVE)
	//Get throttle value from BUS.
	if (display.online){		
		throttle = motor.throttle;
	}else{
		return false;
	}
	#endif
	
	//Override by pressing button in menu 5.
	if (throttle_override > 0){
		throttle = throttle_override;
	}
	return true;
}

/*
	Returns true is brake signal is ok
*/
bool get_brake(void){
	#if(OPT_BRAKE==FW_BRAKE_DIRECT)
	if (ad_brake < THROTTLE_DISC){
		return false;
	}
	if (ad_brake > THROTTLE_OVER){
		return false;
	}
	uint32_t t;
	//This is all fine.
	if (ad_brake <= THROTTLE_LOW){
		brake = 0;
		use_brake = false;
	}else if (ad_brake >= THROTTLE_HIGH){
		use_brake = true;
		brake = 100;		
	}else{
		//In between
		t = 100000 / (THROTTLE_HIGH-THROTTLE_LOW);
		t *= (ad_brake - THROTTLE_LOW);
		brake = t / 1000;
		use_brake = true;
	}
	#endif
	
	//Scale brake power.
	if ((display.online) && (!display.road_legal)){
		/*
		if (display.function_val2 > 0){			
			brake_level = (display.function_val2 + 1);
			brake = (brake / 10) * brake_level;
			if (brake > 100){
				brake = 100;
			}
		}else{
			use_brake = false;
			brake = 0;
		}*/
	}else{
		use_brake = false;
		brake = 0;
	}
	return true;
}


/*
	Read factory calibration from NVM:
*/
/*
void read_calibration(void){	
	//Get the factory temperature value, at 85 Deg. C.		
	ad_temp_ref = SP_ReadCalibrationByte(PRODSIGNATURES_ADCACAL0) ;
	ad_temp_ref |= SP_ReadCalibrationByte(PRODSIGNATURES_ADCACAL1) <<8;
}
*/
int main(void){
	//Run of the internal 32MHz oscillator.
	clock_switch32M();
	
	init_pwm();
	uart_init();	
	bus_init(&bus);	
	
	//Clear the AD settings.
	PORTA.DIR = 0;
	PORTA.OUT = 0;
	ADCA.CTRLA  = ADC_FLUSH_bm;
	ADCA.CTRLB  = 0;
	ADCA.REFCTRL  = 0;
	ADCA.EVCTRL  = 0;
	ADCA.PRESCALER  = 0;	

	//Set PORTB as AD input
	PORTB.DIR = 0;
	PORTB.OUT = 0;
		
	//Setup a timer, just for counting. Used for bus message and timeouts.
	TCC1_CTRLA = TC_CLKSEL_DIV1024_gc;
	
	//Set up the RTC for speed timing.
	CLK.RTCCTRL = CLK_RTCSRC_RCOSC32_gc | CLK_RTCEN_bm;	
	RTC.CTRL = RTC_PRESCALER_DIV1_gc;
	RTC.COMP = 32768;
	
	//A counter for 100% PWM timing.
	TCD0.CTRLA = TC_CLKSEL_DIV1024_gc;
	TCD0.PER = 320;
	TCD0.INTCTRLA = TC_OVFINTLVL_HI_gc;
	
	
	//Make sure to move the reset vectors to application flash.
	//Set the interrupts we want to listen to.
	CCP = CCP_IOREG_gc;
	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm |PMIC_HILVLEN_bm | PMIC_RREN_bm;
	
	//Enable interrupts.
	sei();
	
	//Set defaults:
	eepsettings.straincal = 200;	
	eemem_read_block(EEMEM_MAGIC_HEADER_SETTINGS,(uint8_t*)&eepsettings, sizeof(eepsettings), EEBLOCK_SETTINGS1);
	
	//Apply settings...
	strain_threshhold = eepsettings.straincal;

	//Startup hall state.
	hall_state = 0x04;
	get_hall();

	//Startup PWM value.
	pwm = PWM_SET_MIN;
	
	//Test
	/*
	motor.mode = 1;
	display.road_legal = false;
	display.function_val1 = 4;
	display.function_val2 = 5;
	*/
	
	uint8_t cnt_tm = 0;		//Used by TCC1
	uint8_t poll_cnt = 0;	//Used for polling the display.	
	uint16_t pwm_lim = 0;	//PWM limit may change at higher speed.
	
	display.strain_th = strain_threshhold;	
	
	//Two flags that can only be set.
	bool should_freewheel = true;
	bool force_commute = false;
	while(1){		
		//Double check the PWM hasn't gone crazy:
		if (pwm < pwm_set_min){
			pwm = pwm_set_min;
		}
		if (pwm > pwm_set_max){
			pwm = pwm_set_max;
		}		

		//Apply PWM settings.
		TCC0.CCA = pwm;
		TCC0.CCB = pwm;
		TCC0.CCC = pwm;
		
		//Get halls sensors.		
		if (!get_hall()){
			should_freewheel = true;			
			status_set(STAT_HALL_FAULT);
		}		
				
		//Commute or freewheel:
		if (should_freewheel){
			pwm_freewheel();
		}else if (force_commute){			
			if (direction == FORWARD){				
				commute_forward();
			}else{
				commute_backward();
			}	
			
		}else if (flag_hall_changed){
			if (direction == FORWARD){
				commute_forward();
			}else{
				commute_backward();
			}
		}else{			
			//pwm_freewheel();
		}
				
		//Remember...
		pwm_prev = pwm;		
		
		//Reset 
		should_freewheel = false;
		//force_commute = false;
				
		if (startup == 10){ //Just started
			//Setup PWM
			commute_start();
			pwm = estimate_pwm_byspeed();
			//Apply PWM settings.
			TCC0.CCA = pwm;
			TCC0.CCB = pwm;
			TCC0.CCC = pwm;
		}
		
		if (display.online == false){
			should_freewheel = true;			
		}		
		if (motor.mode == 0){
			should_freewheel = true;
		}
		
		//Get voltage,current and whatever else.
		get_measurements();
		
		//Instantanious under and over voltage.
		if (ad_voltage > 3700){ //3900
			//Freewheel as fast as possible.
			pwm_freewheel();
			should_freewheel = true;
			status_set(STAT_OVER_VOLTAGE);
		}else if (ad_voltage < 1800){ //About 19 volts.
			//Freewheel as fast as possible.
			pwm_freewheel();
			should_freewheel = true;
			status_set(STAT_UNDER_VOLTAGE);
			pwm_less(200);pwm_less(200); //OMG!
		}

		//Regenning too hard:
		if (ad_current_regav < -1300){
			//Freewheel as fast as possible.
			//pwm_freewheel();
			//should_freewheel = true;
			//status_set(STAT_REGEN_HIGH);
			//startup = 3;
		}		
		
		if (use_throttle){
			//Get the throttle value.
			if (!get_throttle()){
				status_set(STAT_THROTTLE_FAULT);
				should_freewheel = true;
			}
		}
		if (use_brake){		
			//Use the brake too:
			if (get_brake()){
				if (use_brake){
					throttle_cruise = false;
					throttle_tick = 0;
				}else{
					if (throttle == 0){ //< 5
						should_freewheel = true;
					}
				}
			}else{
				status_set(STAT_BRAKE_FAULT);				
				should_freewheel = true;
			}
		}		
		
		//Maybe it'll run without at some point...
		if (usehall){
			//Timer:
			if (flag_hall_changed){
				//Toggle blue led:
				PORTC.OUTTGL = PIN7_bm;			
			
				//compute time:
				time_now = RTC.CNT;			
				time_comm = time_now;
				//Running average.			
				time_comm_av += time_comm;
				time_comm_av >>= 1;
				
				commtime_sum+= time_comm;
				commtime_cnt++;				
				cnt_commutations++;				
			
				//Reset timer.
				RTC.CNT = 0;

				//Computer average current in this time:
				if (ad_current_regcnt > 0){
					ad_current_regav = ad_current_regsum / (int32_t)ad_current_regcnt;
				}else{
					ad_current_regav = ad_current;
				}

				//Values reset in next if-statement.
			}
			
			//Allowed to go faster if the commutation time is guaranteed to switch fast enough.			
			if ((time_comm_av < 350)&&display.online && (display.function_val2==9)){				
				#if((HARDWARE_VER==HW_BLDC_REV0)||(HARDWARE_VER==HW_BLDC_REV1))
				//TCD0 is used in case the motor suddenly stops. Capacitors are 100uF/16V
				pwm_set_min = 0;
				#else
				//PWM is inverted on this board.
				pwm_set_max = PWM_SET_MAX;
				#endif
			}
			
			//PWM limit for braking
			pwm_lim = pwm_set_max -  (display.function_val3 * 10);
			if (pwm_lim < pwm_set_min){
				pwm_lim = pwm_set_min;
			}else if (pwm_lim > pwm_set_max){
				pwm_lim = pwm_set_max;
			}
			
			//That's really slow.
			if (RTC.CNT> 5000){			
				speed = 0;				
			}
		
			//Minimum response time
			if (flag_hall_changed) {
					

				//Clear current sum:
				ad_current_regcnt = 0;
				ad_current_regsum = 0;				
				
				RTC.CNT = 0;	
			}							
			
			//Control loop
			if ((flag_hall_changed) || (TCC1.CNT > 450)){
				//Clear hall flag if it was set.
				flag_hall_changed = false;
				if (!off){
					//If brake is enabled:
					if (use_brake){
						if (brake > 5){
							//Regenning is a bit harder. The amount of strength decreases if PWM is too high.
							//Brake is current limited.
							//At low speed, ovverride PWM settings
							if (speed < 150){
								pwm_lim = pwm_set_max;
							}
																
							if ((ad_current_regav + 169) > (((int16_t)brake)*-12)){  //100 * 10 -> current of about -8 Amp
								//More power: inc PWM
									
								//Use the speed average as a voltage estimate for the motor.
								//24V will give an unloaded speed of 350 (35.0kmh)
								//If we're doing 10km/h, the motor is at 7V approx, 30% duty cycle.
									
								if (pwm < pwm_lim){
									pwm_less(slope_brake);
								}									
									
							}else{
								//Back off
								pwm_more(slope_brake);
							}
							//Voltage limits
							if (ad_voltage > 3600){
								pwm_more(slope_brake/2);
							}
							if (ad_voltage > 3700){
								pwm_more(slope_brake/2);
							}
						}
					}
					if (use_throttle){
						//Undo brake :
						if (flag_brake_lowrpm){
							off = false;
							tie = false;
							status_clear();
							flag_brake_lowrpm = false;
						}

						if (throttle > 5){
							//Voltage limits
							if (ad_voltage < 1950){
								pwm_less(slope_throttle*2);
							}	
							if (ad_voltage < 1900){
								pwm_less(slope_throttle*2);
							}
							
							//Current measures this commutation cycle.
							int32_t regulation = ad_current_regav; 
							
							//Fixed calibration. Regulation value of 100 = 1 Amp
							#if(HARDWARE_VER == HW_CTRL_REV1)							
							regulation += 1749;
							regulation *= 1000;
							regulation /= 286;
							regulation -= 34;
							#else
							regulation += 169;
							regulation *= 100;
							regulation /= 75;
							regulation += 18;
							#endif
							
							//Target current.
							int16_t target = ((int16_t)(throttle*15)/10)*2*(int16_t)motor.mode;
							
							//Current regulation
							if (1){
								if (regulation < (target/4)){ //200 * 0-5
									pwm_more((speed/10)+1);
								}else if (regulation < (target/2)){
									//More power
									pwm_more(slope_throttle*2);
								}else if (regulation < target){ 
									//More power
									pwm_more(slope_throttle);
								}else{
									//Back off
									pwm_less(slope_throttle);
								}
							}							
						}
					}
					
					if (use_pedalassist) {						
						if (ad_strain_av > highestforce){
							highestforce  = ad_strain_av;
						}
						if (ad_strain_av < lowestforce){
							lowestforce = ad_strain_av;
						}
						
						//Compute 
						pedal_signal_prev = pedal_signal;
						uint8_t pedal_new = 0;
						
						if (ad_strain_av > strain_threshhold){
							//Get the pedal value.
							if (ad_strain_av-strain_threshhold < 100){
								pedal_new = ad_strain_av-strain_threshhold;
							}else{
								pedal_new = 100;
							}
						
							//Reset timer.
							if (pedal_new >= pedal_signal){								
								strain_cnt = 120;
							}else if (pedal_new > 20){
								strain_cnt = 60;	
							}							
						}
						
						//Take the highest as keep it there.
						if (pedal_new > pedal_signal_prev){
							pedal_signal = pedal_new;
						}
						
						
						if (strain_cnt){
							strain_cnt--;
						}else{
							//Gradually slope to 
							if (pedal_signal){
								pedal_signal--;
							}							
						}
						
						if ((pedal_signal > 15) && strain_cnt){
							//Same control loop as in throttle:
							//Voltage limits
							if (ad_voltage < 1950){
								pwm_less(slope_throttle*2);
							}	
							if (ad_voltage < 1900){
								pwm_less(slope_throttle*2);
							}
							
							//Current measures this commutation cycle.
							int32_t regulation = ad_current_regav;
							
							//Fixed calibration. Regulation value of 100 = 1 Amp
							#if(HARDWARE_VER == HW_CTRL_REV1)
							regulation += 1749;
							regulation *= 1000;
							regulation /= 286;
							regulation -= 34;
							#else
							regulation += 169;
							regulation *= 100;
							regulation /= 75;
							regulation += 18;
							#endif
							
							//Target current.
							int16_t target = ((int16_t)(pedal_signal*15)/10)*2*(int16_t)motor.mode;
							
							//Current regulation
							if (1){
								if (regulation < (target/4)){
									pwm_more((speed/10)+1);
								}else if (regulation < (target/2)){
									//More power
									pwm_more(slope_throttle*2);
								}else if (regulation < target){ 
									//More power
									pwm_more(slope_throttle);
								}else{
									//Back off
									pwm_less(slope_throttle);
								}
							}			
						}						
					}
				}else{ //if (off)
					//Undo braking at low RPM
					if (flag_brake_lowrpm){
						off = false;
						tie = false;
						status_clear();
						flag_brake_lowrpm = false;
					}					
				}
			}			
		}else{ //Hall hasn't changed.
			//Don't use hall.
			if (RTC.CNT > time_comm_av_last){
				//Reset timer.
				RTC.CNT = 0;				
			}					
		}		
		
		//Act on a fault?
		if (isfaultset()){
			if (tie){	
				tie_ground();
			}else{			
				should_freewheel = true;
			}			
		}
		
		if (use_pedalassist && (!use_throttle)){
			if (!pedal_signal){
				should_freewheel = true;
			}
		}else if ((!use_pedalassist) && (use_throttle && (throttle < 5))){
			should_freewheel = true;
		}else if ((use_throttle)&& (use_pedalassist)){
			if ((throttle < 5) && (!pedal_signal)){
				should_freewheel = true;
			}
		}
		
		//Current min/max
		if (ad_current < ad_currentmin){
			ad_currentmin = ad_current;
		}
		if (ad_current > ad_currentmax){
			ad_currentmax = ad_current;
		}
		
		if (flag_keep_pwm){
			pwm = pwm_prev;
		}

		//Counter for speed.
		cnt_rotations = cnt_commutations / 48;
				
		//Set display data	
		display.voltage = ad_voltage_av;
		display.current = ad_current_av;
		display.power = power_av;
		display.error = status;
		display.speed = speed% 1000; //999 max
		motor.status = status;
		//display.speed = pwm;
		//display.current = pwm_lim;
		if (use_brake){
			display.throttle = brake;	
		}else{
			if (use_pedalassist){
				if (pedal_signal > throttle){
					display.throttle = pedal_signal;
				}else{
					display.throttle = throttle;
				}
			}else{
				display.throttle = throttle;
			}
			
		}
		display.cruise = throttle_cruise;
		
		//About every 14 ms.
		if (TCC1.CNT > 450){
			cnt_tm++;
			TCC1.CNT = 0;
			
			//Apply PWM limit:
			#if(HARDWARE_VER==HW_CTRL_REV1)
			pwm_set_max = (display.function_val2 +1) * 50;
			if (pwm_set_max > PWM_SET_MAX){
				pwm_set_max = PWM_SET_MAX;
			}
			#else
			pwm_set_min = (display.function_val2 +1) * 50;
			if (pwm_set_max > PWM_SET_MAX){
				pwm_set_max = PWM_SET_MAX;
			}
			#endif			
			
			//Throttle from menu.
			#if(HARDWARE_SUPPORTS_DISPLAY)
			if (display.online && (display.func == 0) &&(!display.road_legal) && (display.menu_downcnt > 30)){
				
				if (throttle_override < 100){
					throttle_override++;
				}
			}else{
				throttle_override = 0;
			}
			#endif
			
			//Startup from standstill			
			if (throttle && (speed == 0)){
				force_commute = true;
				//pwm_less(20);
			}else{
				force_commute = false;
			}			
			
			if (!motor.mode){
				pwm_less(200);
			}			
			
			//Last character in message
			if (wait_for_last_char){
				wait_for_last_char = false;
			}else{
				bus_endmessage(&bus);
			}			
			
			//Estimate PWM value by speed. Only when there is no throttle signal or brake.
			if (!use_brake){
				if ((use_throttle && (throttle < 5)) && ((use_pedalassist)&&(!pedal_signal))){
					pwm = estimate_pwm_byspeed();					
				}
			}
			
			//Speed is now the amount of commutations per seconds.
			if (commtime_sum){
				speed = (commtime_cnt * 32768UL) / commtime_sum;
				//With a 26" wheel:
				speed = (speed * 10000UL) / 6487UL;
			}else{
				speed=0;
			}
			
			//Send timer tick
			bus_tick(&bus);
			
			if (bus_check_for_message()){
				green_led_on();				
			}else{
				green_led_off();
				//Increase offline count.
				display.offline_cnt++;
				if (display.offline_cnt > 10){
					uart_rate_find();
					PORTC.OUTSET = PIN6_bm;
				}else{
					PORTC.OUTCLR = PIN6_bm;
				}					
				
				//If offline too long:
				if (display.offline_cnt > 50){
					//You need to make it unsafe again.
					display.road_legal = true;
					display.online = false;
					throttle_cruise = false;
					should_freewheel = true;
					throttle = 0;
					use_brake = false;
					brake = 0;
					pedal_signal = 0;
					strain_cnt = 0;
					motor.mode = 0;
				}
				
				poll_cnt++;				
				if (poll_cnt == 2){
					#if (OPT_DISPLAY== FW_DISPLAY_MASTER)
					bus_display_poll(&bus);
					#endif
				}else if (poll_cnt == 4){
					//Update the display, and it's variables:									
					
					commtime_sum = 0;
					commtime_cnt = 0 ;
						
					//Disaplay average voltage:
					ad_voltage_av = ad_voltage_sum / meas_cnt;
					ad_voltage_sum = 0;
						
					//And current.
					ad_current_av = ad_current_sum / meas_cnt;
					ad_current_sum = 0;

					//Strain, if we have it:
					#if(HARDWARE_HAS_STRAIN)
					//Running average:
					int32_t strain_val = ad_strain_sum / meas_cnt;
					strain_val /= 100;
					
					ad_strain_av += strain_val;
					ad_strain_av /= 2;
					
					//ad_strain_av = meas_cnt;
					ad_strain_sum = 0;
					
					//Strain calibration in Menu F8/F3:
					if ((display.func == 8) && (display.menu_downcnt > 100)){	
						strain_threshhold = 200;
						display.strain_th = strain_threshhold;
					}						
					if ((display.func == 3) && (display.menu_downcnt > 50)){						
						if (ad_strain_av > strain_threshhold){
							strain_threshhold+=strain_cal_inc;
						}
						display.strain_th = strain_threshhold;
						flagsave = true;
						
						if (display.menu_downcnt > 100){
							strain_cal_inc = 5;
						}
					}else if  ((display.func == 3) && (display.button_state_prev == 0) && (display.button_state == BUTT_MASK_FRONT)){
						strain_threshhold += 1;
						display.strain_th = strain_threshhold;
					}
					
					if (display.menu_timeout == 0){
						if (flagsave){
							flagsave = false;
							//Save
							eepsettings.straincal = strain_threshhold;
							eemem_write_block(EEMEM_MAGIC_HEADER_SETTINGS,(uint8_t*)&eepsettings, sizeof(eepsettings), EEBLOCK_SETTINGS1);							
						}						
					}
					
					//motor.current = ad_strain_av;
					display.value1 = ad_strain_av;// ad_strain_av - 2000;
					display.value2 = highestforce;// ad_strain_av - 2000;
					#endif
					
					display.value3 = pwm;
					display.value4 = should_freewheel;
					
					//Temperature
					ad_temp_av = ad_temp_sum / meas_cnt;
					ad_temp_sum = 0;
					
					//Set test value
					//display.value2 = ad_temp_av / 2;
					
					//Temperature about 20 Deg.C is 1866 counts. CKDIV16 2020 CKDIV512
					
										
					//Counts per kelvin.
					/*
					uint32_t cpk = ad_temp_ref*10 / 358;					
					ad_temp_av  = (ad_temp_av * 10) / cpk - 273;
					ad_temp_av = ad_temp_ref;
					*/
												
					meas_cnt = 0;
						
					//Calibration:
					ad_voltage_av -= 175;
					ad_voltage_av *= 1000;
					ad_voltage_av /= 886;
					
					//Current calibration:	
					#if(HARDWARE_VER == HW_CTRL_REV1)
					ad_current_av += 1749;
					ad_current_av *= 1000;
					ad_current_av /= 286;
					ad_current_av -= 34;
					#else					
					ad_current_av += 169;
					ad_current_av *= 100;
					ad_current_av /= 75;
					ad_current_av += 18;				
					#endif
					
					power_av = (ad_current_av * ad_voltage_av) / 10000;
									
					poll_cnt = 0;
					#if(OPT_DISPLAY == FW_DISPLAY_MASTER)
					bus_display_update(&bus);
					#endif
					//motor.mode = 1;
				}
			}
		}
	
		//cnt_tm is incremented from software TCC1 counts. This should happen ~ each 220 ms.
		if (cnt_tm > 15){
			cnt_tm = 0;
			
			//Increase throttle tick, for cruise control.
			if ((use_throttle)&& (!use_brake)){
				if ((display.online)&&(throttle > 10)){
					if (throttle_tick < 20){
						throttle_tick++;
					}else{
						//throttle_cruise = true; //Removed it for the time being.
					}
				}				
			}
			
			//Reset these?		
			ad_currentmin = 9999;
			ad_currentmax = -9999;			
			
			//Startup is set... when we've just started.
			if (startup){
				startup--;				
				if (startup == 0){
					//Clear any status flag:
					//status_clear();
					//Give it a nudge.
					//commute_allowed = true;
					pwm_less(250);
				}
			}else{
				status_clear();
				
			}			
		}
		
		//Set fault flags:
		if (off){
			status_set(STAT_STOPPED);
		}
		
		if (use_throttle){
			//status_set(STAT_USETHROTTLE);
		}		
	}
}


bool bus_check_for_message(void){
	if (bus.new_mesage){
		#if(OPT_DISPLAY == FW_DISPLAY_MASTER)
		if (bus_parse_battery(&bus,bus.msg_buff,bus.msg_len)){
		#else		
		if (bus_parse_motor(&bus,bus.msg_buff,bus.msg_len)){
		#endif
			bus.msg_len = 0;
			bus.new_mesage = false;
			return true;
		}else{
			bus.msg_len = 0;
			bus.new_mesage = false;
			return false;
		}
	}
	return false;
}


bool isfaultset(void){
	if (status & STAT_ERROR_MASK){
		return true;
	}else{
		return false;
	}
}

/*
	Used for setting the PWM limit, forcing the high sides to reload the gate drive cap.
*/
ISR(TCD0_OVF_vect){
	pwm_set_min = 35;
	
	if (pwm < pwm_set_min){
		pwm = pwm_set_min;
	}
}


void pwm_more(uint8_t by){
	#if((HARDWARE_VER == HW_BLDC_REV1)||(HARDWARE_VER == HW_BLDC_REV0))
	if (pwm > pwm_set_min){
		if ((pwm-pwm_set_min) < by){
			pwm = pwm_set_min;
		}else{
			pwm -= by;
		}
	}else{
		pwm = pwm_set_min;
	}
	#else
	if (pwm < pwm_set_max){
		if ((pwm_set_max-pwm) < by){
			pwm = pwm_set_max;
		}else{
			pwm += by;
		}
	}else{
		pwm = pwm_set_max;
	}
	#endif
}

void pwm_less(uint8_t by){
	#if((HARDWARE_VER == HW_BLDC_REV1)||(HARDWARE_VER == HW_BLDC_REV0))
	if (pwm < pwm_set_max){
		if ((pwm_set_max-pwm) < by){
			pwm = pwm_set_max;
		}else{
			pwm += by;
		}
	}else{
		pwm = pwm_set_max;
	}
	#else
	if (pwm > pwm_set_min){
		if ((pwm-pwm_set_min) < by){
			pwm = pwm_set_min;
		}else{
			pwm -= by;
		}
	}else{
		pwm = pwm_set_min;
	}
	#endif
}

//Clear status flags.
void status_clear(void){
	status = 0;
}

//Set em.
void status_set(uint32_t flags){
	status |= flags;
	startup = 9;
}

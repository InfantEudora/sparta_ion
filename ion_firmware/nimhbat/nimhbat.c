/*
	NIMHBAT.c
	
	Created: 7/1/2014 6:42:19 PM
	Author: Dick Prins
	
	Runs on test PCB, which has input for throttle and brake.
	This PCB controls the display, and retrieves/sets motor state. 	
 */
#include <avr/io.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <avr/interrupt.h>
#include "app.h"
#include "adc.h"
#include "uart.h"
#include "../../lib_ion/bowbus.h"

#include "sp_driver.h"
#include "flash_wrapper.h"

bool use_throttle = false;		//Apply throttle.
bool use_brake = false;			//Apply brake


volatile uint8_t brake = 0;		//0 = no regen, 100 = max regen.
volatile uint8_t throttle = 0;
volatile uint8_t brake_level = 0;	//Brake power

//bitmaps for status flags
#define STAT_OK					0
#define STAT_THROTTLE_FAULT		1
#define STAT_OVER_VOLTAGE		2
#define STAT_UNDER_VOLTAGE		4
#define STAT_REGEN_HIGH			8
#define STAT_STOPPED			16
#define STAT_USETHROTTLE		32
#define STAT_HALL_FAULT			64
#define STAT_BRAKE_FAULT		128

//All the erros 
#define STAT_ERROR_MASK			(STAT_THROTTLE_FAULT | STAT_BRAKE_FAULT | STAT_OVER_VOLTAGE | STAT_UNDER_VOLTAGE | STAT_REGEN_HIGH | STAT_STOPPED )

//Status bitmask
uint32_t status = 0;

//Functions:
void status_set(uint32_t);
void status_clear(void);
bool isfaultset(void);



bool get_throttle(void);
bool get_brake(void);

//For Bowbus
bool bus_check_for_message(void);

uint32_t ad_strain = 0;
uint32_t ad_strain_sum = 0;
uint32_t ad_strain_av = 0;

uint32_t ad_temp_ref = 0;	//Factory reference.
uint32_t ad_temp = 0;
uint32_t ad_temp_sum = 0;
uint32_t ad_temp_av = 0;

uint32_t ad_throttle = 0;
uint32_t ad_brake = 0;

uint32_t ad_voltage = 0;
uint32_t ad_voltage_sum = 0;
uint32_t ad_voltage_av = 0;

uint16_t ad_current_regcnt = 0;
int32_t ad_current_regsum = 0;		//Average computed per commutation
int32_t ad_current_regav = 0;		//The average for the commutation loop.

uint16_t meas_cnt = 0;

int32_t ad_currentmin = 0;
int32_t ad_currentmax = 0;

int32_t val_current;
int32_t val_voltage;

uint32_t commtime_sum = 0;	//Sum of commutation times.
uint32_t commtime_cnt = 0;	//Number of commutations within that period
uint32_t speed = 0;			//in kmh * 1000. So 10000 = 10.0 kmh


//Regulaton settings
uint8_t slope_throttle = 10;	//Rate at which PWM may increase for throttle.
uint8_t slope_brake = 10;		//Rate at which PWM may increase for brake.


#define THROTTLE_DISC	600		//Throttle disconnected. //600 safe value
#define THROTTLE_LOW	1100		//Lowest
#define THROTTLE_HIGH	3855	//Highest
#define THROTTLE_OVER	3900	//Too high //3100 safe value

uint16_t throttle_tick = 0;
bool throttle_cruise = false;


bowbus_net_s bus;


void clock_switch32M(void){
	/*Setup clock to run from Internal 32MHz oscillator.*/
	OSC.CTRL |= OSC_RC32MEN_bm;
	while (!(OSC.STATUS & OSC_RC32MRDY_bm));
	CCP = CCP_IOREG_gc;
	CLK.CTRL = CLK_SCLKSEL_RC32M_gc;	
	OSC.CTRL &= (~OSC_RC2MEN_bm);
	PORTCFG.CLKEVOUT = (PORTCFG.CLKEVOUT & (~PORTCFG_CLKOUT_gm)) | PORTCFG_CLKOUT_OFF_gc;	// disable peripheral clock	
}


//Get all analog samples on this board. 
uint8_t ad_max_samples = 8;

void get_measurements(void){
	uint32_t usample = 0;
	uint32_t gsample = 0;
	uint32_t bsample = 0;
	int32_t isample = 0;

	//Enable lower FETs
	#if(HARDWARE_VER == HW_CTRL_REV0)
		//This board does not have brake/throttle, but PAS.
		for (uint16_t i=0;i<ad_max_samples;i++){
			adc_init_single_ended(REF_3V3,ADC_CH_MUXPOS_PIN7_gc); //Bat Voltage.
			usample += adc_getsample();
			adc_init_differential(REF_3V3,ADC_CH_MUXPOS_PIN8_gc); //Strain Sensor.
			gsample += adc_getsample();	
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

	//Hardware specific inputs.
	#if(HARDWARE_VER == HW_CTRL_REV0)
		ad_strain = gsample/ad_max_samples;
		ad_strain_sum += ad_strain;
		
		ad_temp = bsample/ad_max_samples;
		ad_temp_sum += ad_temp;
	#else
		ad_throttle = gsample/ad_max_samples;
		ad_brake = bsample/ad_max_samples;
	#endif
}


/*
	Returns true if ad range is in bounds.
*/
bool get_throttle(void){
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
	
	/*
	//If we are cruising:
	if (throttle_cruise){
		throttle = 100;
	}		
	
	//Scale:
	if ((display.online) && (!display.road_legal)){
		if ((motor.mode > 0) && (motor.mode < 6)){
			uint8_t s = motor.mode;
			throttle = (throttle / 5) * s;
		}else{
			throttle = 0;
		}
	}else{
		throttle = 0;
	}*/
	
	
	
	return true;	
}

/*
	Returns true is brake signal is ok
*/
bool get_brake(void){
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
	
	//Scale brake power.
	if ((display.online) && (!display.road_legal)){
		if (display.function_val2 > 0){			
			brake_level = (display.function_val2 + 1);
			brake = (brake / 10) * brake_level;
			if (brake > 100){
				brake = 100;
			}
		}else{
			use_brake = false;
			brake = 0;
		}
	}else{
		use_brake = false;
		brake = 0;
	}
	return true;
}


/*
	Read factory calibration from NVM:
*/
void read_calibration(void){	
	//Get the factory temperature value, at 85 Deg. C.		
	ad_temp_ref = SP_ReadCalibrationByte(PRODSIGNATURES_ADCACAL0) ;
	ad_temp_ref |= SP_ReadCalibrationByte(PRODSIGNATURES_ADCACAL1) <<8;
}




int main(void){
	clock_switch32M();
	
	uart_init();	
	bus_init(&bus);	

	
	PORTC.DIR = 0xFF;
		
	//Setup a timer, just for counting. Used for bus message and timeouts.
	TCC1_CTRLA = TC_CLKSEL_DIV1024_gc;

	//Set the interrupts we want to listen to.
	PMIC.CTRL = PMIC_MEDLVLEN_bm |PMIC_HILVLEN_bm | PMIC_RREN_bm;
		
	//Make sure to move the reset vectors to application flash.
	CCP = CCP_IOREG_gc;
	PMIC.CTRL &= ~PMIC_IVSEL_bm;
	
	//Enable interrupts.
	sei();
	
	//Read temperature calibration:
	read_calibration();
		
	uint8_t cnt_tm = 0;		//Used by TCC1
	uint8_t poll_cnt = 0;	//Used for polling the display.	
	//uint16_t pwm_lim = 0;	//PWM limit may change at higher speed.
	
	while(1){
		continue;
		get_measurements();
		if (get_throttle()){
			motor.throttle = throttle;
		}else{
			motor.throttle = 0;
		}
		
		if (display.online == false){
			motor.mode = 0;
		}
			
		//Set display data	
		display.voltage = motor.voltage;
		display.current = motor.current;
		//display.power = power_av;
		display.throttle = throttle;
		display.error = status | motor.status;
		/*if (motor.status){
			display.error = motor.status;	
		}*/
		//display.speed = speed% 1000; //999 max
		
		if (TCC1.CNT > 500){
			cnt_tm++;
			TCC1.CNT = 0;
		
			
			//Last character in message
			if (wait_for_last_char){
				wait_for_last_char = false;
			}else{
				bus_endmessage(&bus);
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
					throttle = 0;
					use_brake = false;
					brake = 0;
				}
				if (motor.offline_cnt > 20){
					motor.online = false;
				}
				
				
				poll_cnt++;				
				if (poll_cnt == 2){					
					bus_display_poll(&bus);	
				}else if (poll_cnt == 4){
					bus_display_update(&bus);
				}else if (poll_cnt == 6){					
					bus_motor_poll(&bus);
					motor.offline_cnt++;					
				}else if (poll_cnt == 8){
					poll_cnt = 0;
				}
			}
		}		
	}
}


bool bus_check_for_message(void){
	if (bus.new_mesage){		
		if (bus_parse_battery(&bus,bus.msg_buff,bus.msg_len)){			
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



//Clear status flags.
void status_clear(void){
	status = 0;
}

//Set em.
void status_set(uint32_t flags){
	status |= flags;
}

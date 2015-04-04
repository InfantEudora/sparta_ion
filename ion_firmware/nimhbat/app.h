/*
*
*
*
*/
#ifndef __BLDC_APP__
#define __BLDC_APP__

#define F_CPU 32000000UL
#include <util/delay.h>

//Defines for different hardware versions.
#define HW_BLDC_REV0		1		//1St BLDC controller with onboard Mosfets, No Display
#define HW_BLDC_REV1		2		//Seconds BLDC controller with onboard Mosfets, Display
#define HW_CTRL_REV0		3		//REV1 -> Round board: Control only: talks directly to display.
#define HW_CTRL_REV0_BAT	4		//REV1 -> Round board: Listens to battery.

//RED LED
#define green_led_on()		PORTC.OUTSET = PIN7_bm;
#define green_led_off()		PORTC.OUTCLR = PIN7_bm;

#define REF_1V0				ADC_REFSEL_INT1V_gc
#define REF_3V3				ADC_REFSEL_INTVCC_gc

//Set this to the version you have.
#define HARDWARE_VER		HW_BLDC_REV1

//Define what periphrials this hardware supports.
#if (HARDWARE_VER == HW_CTRL_REV0)
	#define HARDWARE_HAS_STRAIN
	#define HARDWARE_HAS_DISPLAY
#elif (HARDWARE_VER == HW_BLDC_REV0)
	#define HARDWARE_HAS_THROTTLE	
#elif (HARDWARE_VER == HW_BLDC_REV1)
	#define HARDWARE_HAS_THROTTLE
	#define HARDWARE_HAS_BRAKE
	#define HARDWARE_HAS_DISPLAY
#endif

extern uint16_t testvalue;

#endif

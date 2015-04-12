/*
	Application defines for bldcdisp.c
*/
#ifndef __BLDC_APP__
#define __BLDC_APP__
/*
	Target is ATxmega32A4U with the following fuse settings:
	FUSEBYTE1 = 0x00
	FUSEBYTE2 = 0xBE
	FUSEBYTE4 = 0xFF
	FUSEBYTE5 = 0xE2
	
	WDWP = 8CLK
	WDP = 8CLK
	BOOTRST = BOOTLDR
	TOSCSEL = XTAL
	BODPD = CONTINUOUS
	RSTDISBL = [ ]
	SUT = 0MS
	WDLOCK = [ ]
	BODACT = CONTINUOUS
	EESAVE = [X]
	BODLVL = 2V6
*/

#include <stdint.h>

//Defines for different hardware versions (PCBs)
#define HW_BLDC_REV0		1		//1St BLDC controller with onboard Mosfets, No Display
#define HW_BLDC_REV1		2		//Seconds BLDC controller with onboard Mosfets, Display
#define HW_CTRL_REV1		3		//REV0 or REV1 -> Round board: Control only: talks directly to display.


//Set this to the version you have.
#define HARDWARE_VER		HW_CTRL_REV1

//Define what peripherals this hardware supports.
#if(HARDWARE_VER == HW_BLDC_REV0)
	#define HARDWARE_SUPPORTS_THROTTLE	true
	#define HARDWARE_SUPPORTS_BRAKE		false
	#define HARDWARE_HAS_STRAIN			false
	#define HARDWARE_SUPPORTS_DISPLAY	false
#elif(HARDWARE_VER == HW_CTRL_REV1)
	//Throttle is supported by receiving signal over one wire bus.
	#define HARDWARE_SUPPORTS_THROTTLE	true
	#define HARDWARE_SUPPORTS_BRAKE		false
	#define HARDWARE_HAS_STRAIN			true
	#define HARDWARE_SUPPORTS_DISPLAY	true
#elif(HARDWARE_VER == HW_BLDC_REV1)	
	#define HARDWARE_SUPPORTS_THROTTLE	true
	#define HARDWARE_SUPPORTS_BRAKE		true
	#define HARDWARE_HAS_STRAIN			false
	#define HARDWARE_SUPPORTS_DISPLAY	true
#endif

//Common in all hardware:
#define red_led_on()				PORTC.OUTSET = PIN6_bm;
#define red_led_off()				PORTC.OUTCLR = PIN6_bm;
#define green_led_on()				PORTC.OUTSET = PIN7_bm;
#define green_led_off()				PORTC.OUTCLR = PIN7_bm;


//EEPROM stuff:
#define EEBLOCK_SETTINGS1					2		//Contains user settings.
#define EEMEM_MAGIC_HEADER_SETTINGS			0x1338	//Magic header.

typedef struct settings_t settings_t;
struct settings_t{
	uint16_t straincal;
	uint16_t uvcal;
	uint16_t ovcal;
	uint16_t tempcal;
	uint8_t  straingain;
};

extern settings_t eepsettings;



//Define what you want the hardware to do:
#define FW_DISPLAY_MASTER		0		//This device updates the display.
#define FW_DISPLAY_SLAVE		1		//This device only listens to display.
#define FW_DISPLAY_NONE			2		//Don't talk to the display ever.

#define FW_THROTTLE_DIRECT		0		//Use onboard throttle input
#define FW_THROTTLE_MASTER		1		//Use onboard throttle input and put it on the bus.
#define FW_THROTTLE_SLAVE		2		//Listen to throttle on the bus.
#define FW_THROTTLE_NONE		3		//No throttle.

#define FW_BRAKE_DIRECT			0		//Use onboard brake input
#define FW_BRAKE_MASTER			1		//Use onboard brake input and put it on the bus.
#define FW_BRAKE_SLAVE			2		//Listen to brake on the bus.
#define FW_BRAKE_NONE			3		//No brake

#define FW_STRAIN_DIRECT		0		//Use onboard strain input
#define FW_STRAIN_MASTER		1		//Use onboard strain input and put it on the bus.
#define FW_STRAIN_SLAVE			2		//Listen to strain on the bus.
#define FW_STRAIN_NONE			3		//Listen to strain on the bus.

#define OPT_DISPLAY				FW_DISPLAY_MASTER
#define OPT_THROTTLE			FW_THROTTLE_SLAVE
#define OPT_BRAKE				FW_BRAKE_SLAVE
#define OPT_STRAIN				FW_STRAIN_DIRECT

extern uint16_t testvalue;

#endif

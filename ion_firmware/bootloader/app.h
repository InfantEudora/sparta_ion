/*
	Application defines for ionblapp.c
*/
#ifndef __BLDC_APP__
#define __BLDC_APP__
#include <stdint.h>
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

//Memory section before the bootloader flash:
#define _PREBOOT_SECTION	__attribute__((__section__(".PREBOOT")))__attribute__((__used__))

//Bootloader is compiled with the following linker flags for ATxmega32A4U:
//-Wl,--section-start=.text=0x8000 -Wl,--section-start=.PREBOOT=0x7000

//Defines for different hardware versions.
#define HW_BLDC_REV0		1		//1St BLDC controller with onboard Mosfets, No Display
#define HW_BLDC_REV1		2		//Seconds BLDC controller with onboard Mosfets, Display
#define HW_CTRL_REV0		3		//REV1 -> Round board: Control only: talks directly to display.

//Set this to the version you have.
#define HARDWARE_VER		HW_CTRL_REV0

#if (HARDWARE_VER == HW_BLDC_REV1)
 //Running as button board.
 #define BOOT_ADDRESS	3
 #define DEVICE_ADDRESS	80
#elif (HARDWARE_VER == HW_CTRL_REV0)
 #define BOOT_ADDRESS	4
 #define DEVICE_ADDRESS	00
#endif

//Define what periphrials this hardware supports.
#if (HARDWARE_VER == HW_CTRL_REV0)
#define HARDWARE_HAS_STRAIN
#define HARDWARE_HAS_DISPLAY
#define green_led_on()		PORTC.OUTSET = PIN7_bm;
#define green_led_off()		PORTC.OUTCLR = PIN7_bm;
#define green_led_tgl()		PORTC.OUTTGL = PIN7_bm;

#define red_led_on()		PORTC.OUTSET = PIN6_bm;info3
#define red_led_off()		PORTC.OUTCLR = PIN6_bm;
#define red_led_tgl()		PORTC.OUTTGL = PIN6_bm;
#elif (HARDWARE_VER == HW_BLDC_REV0)
#define HARDWARE_HAS_THROTTLE

#elif (HARDWARE_VER == HW_BLDC_REV1)
#define HARDWARE_HAS_THROTTLE
#define HARDWARE_HAS_BRAKE
#define HARDWARE_HAS_DISPLAY
#define green_led_on()		PORTC.OUTSET = PIN7_bm;
#define green_led_off()		PORTC.OUTCLR = PIN7_bm;
#define green_led_tgl()		PORTC.OUTTGL = PIN7_bm;

#define red_led_on()		PORTC.OUTSET = PIN6_bm;
#define red_led_off()		PORTC.OUTCLR = PIN6_bm;
#define red_led_tgl()		PORTC.OUTTGL = PIN6_bm;
#endif

//ATMEL's page/word definitions are a bit broken...

//EEPROM block numbers:
#define EEBLOCK_HARDWARE				0		//Contains hardware data.
#define EEBLOCK_FIRMWARE				1		//Contains firmware data.
#define EEBLOCK_SETTINGS1				2		//Contains user settings.

//Constants at the start of an EEPROM block:
#define EEMEM_MAGIC_HEADER_HARDWARE			0xB0B1	//Magic header.
#define EEMEM_MAGIC_HEADER_FIRMWARE			0xDEAD	//Magic header.
#define EEMEM_MAGIC_HEADER_SETTINGS			0x1338	//Magic header.

//Data structures stored in EEPROM:
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

//Defined in ionblapp.c
extern hwinfo_t hwinfo;
extern fwinfo_t fwinfo;

#endif

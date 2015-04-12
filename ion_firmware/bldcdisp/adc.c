/*
 * adc.c
 *
 * Created: 10/16/2013 2:10:35 PM
 *  Author: Dick
 */ 

#include "adc.h"


/*Simple single ended input setup*/
void adc_init_single_ended(uint8_t ref, uint8_t channel){
	PORTA.DIR = 0;	 // configure PORTA as input
	ADCA.CTRLA = 1;	 // enable adc
	ADCA.CTRLB = ADC_RESOLUTION_12BIT_gc;	 // 12 bit conversion
	ADCA.REFCTRL = ref;	 // internal 1V bandgap reference
	ADCA.PRESCALER = ADC_PRESCALER_DIV16_gc;	 // peripheral clk/8 (2MHz/16=250kHz)
	ADCA.CH0.CTRL = ADC_CH_INPUTMODE_SINGLEENDED_gc;	 // single ended
	ADCA.CH0.MUXCTRL = channel;	 // PORTA:2
}

/* Works
void adc_init_differential(uint8_t ref){
	PORTA.DIR = 0;	 // configure PORTA as input
	ADCA.CTRLA |= 0x1;                                   // enable adc
	ADCA.CTRLB = 0x10 | ADC_RESOLUTION_12BIT_gc;         // 12 bit signed conversion (pos 11bits)
	ADCA.REFCTRL = ref | 0x02;           // internal 1V bandgap reference
	ADCA.PRESCALER = ADC_PRESCALER_DIV64_gc;             // peripheral clk/16 (2MHz/16=125kHz)
	ADCA.CH0.CTRL = ADC_CH_INPUTMODE_DIFF_gc;            // differential
	ADCA.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN2_gc | ADC_CH_MUXNEG_PIN3_gc;      // PORTA:2 wrt A3
	//ADCA.CH0.INTCTRL = ADC_CH_INTLVL_HI_gc;              // hi level interrupt
	//ADCA.EVCTRL = ADC_EVSEL_0123_gc | ADC_EVACT_CH0_gc;  // trigger ch0 conversion on event0
}*/

void adc_init_differential(uint8_t ref, uint8_t pos_channel){
	PORTA.DIR = 0;	 // configure PORTA as input
	ADCA.CTRLA = 1;                                   // enable adc
	ADCA.CTRLB = 0x10 | ADC_RESOLUTION_12BIT_gc;         // 12 bit signed conversion (pos 11bits)
	ADCA.REFCTRL = ref | 0x02;           // internal 1V bandgap reference
	ADCA.PRESCALER = ADC_PRESCALER_DIV16_gc;            // 32MHz/16 = 2MHz AD clock.  
	ADCA.CH0.CTRL = ADC_CH_INPUTMODE_DIFF_gc;            // differential, 1x gain
	ADCA.CH0.MUXCTRL = pos_channel | 0b00000100;      // int GND;      // PORTA:2 wrt A3
	

}



void adc_init_strain(uint8_t ref){	
	ADCA.CTRLA = 1;                                   // enable adc
	ADCA.CTRLB = 0x10 | ADC_RESOLUTION_12BIT_gc;         // 12 bit signed conversion (pos 11bits)
	ADCA.REFCTRL = ref | 0x02;           // internal 1V bandgap reference
	ADCA.PRESCALER = ADC_PRESCALER_DIV16_gc;            // 32MHz/16 = 2MHz AD clock.
	ADCA.CH0.CTRL = ADC_CH_INPUTMODE_DIFFWGAIN_gc | (6<<2);            // differential, stupid amount of gain	
	ADCA.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN1_gc | ADC_CH_MUXNEG_PIN4_gc; //Difference between pin 1 and 4.
}

//Init temperature measurement: Internal 1V0 ref, 
void adc_init_temperature(void){
	PORTA.DIR = 0;	 // configure PORTA as input
	ADCA.CTRLA = 1;                                   // enable adc
	ADCA.CTRLB = ADC_RESOLUTION_12BIT_gc;         // 12 bit unsigned conversion
	ADCA.REFCTRL = ADC_REFSEL_INT1V_gc | ADC_TEMPREF_bm;	//Temperature and 1V0
	ADCA.PRESCALER = ADC_PRESCALER_DIV16_gc;            // 32MHz/16 = 2MHz AD clock.
	ADCA.CH0.CTRL = ADC_CH_INPUTMODE_INTERNAL_gc;            // differential
	ADCA.CH0.MUXCTRL = ADC_CH_MUXINT_TEMP_gc;      // int GND;      // PORTA:2 wrt A3

}


void adc_init_differential_gain(uint8_t ref,uint8_t gain, uint8_t pos_channel){
	PORTA.DIR = 0;	 // configure PORTA as input
	ADCA.CTRLA = 0x1;//Disable
	ADCA.CTRLB = ADC_CONMODE_bm | ADC_RESOLUTION_12BIT_gc;         // 12 bit signed conversion (pos 11bits)
	ADCA.REFCTRL = ref;           // internal 1V bandgap reference
	ADCA.PRESCALER = ADC_PRESCALER_DIV16_gc;             // peripheral clk/16 (2MHz/16=125kHz)
	ADCA.CH0.CTRL = ADC_CH_INPUTMODE_DIFFWGAIN_gc | (gain & 0x1C);            // differential
	ADCA.CH0.MUXCTRL =  pos_channel | 0b00000111;      // int GND
	
	//Set channel 1 to sample voltage input always.
	//ADCA.CH1.CTRL = ADC_CH_INPUTMODE_DIFFWGAIN_gc | (gain & 0x1C);
	//ADCA.CH1.MUXCTRL =  ADC_CH_MUXPOS_PIN0_gc | ADC_CH_MUXNEG_PIN4_gc;
	
	//ADCA.CH0.INTCTRL = ADC_CH_INTLVL_HI_gc;              // hi level interrupt
	//ADCA.EVCTRL = ADC_EVSEL_0123_gc | ADC_EVACT_CH0_gc;  // trigger ch0 conversion on event0
	ADCA.CTRLA |= 0x1;                                   // enable adc
	ADCA.INTFLAGS = 0x0F;	//Clear flags
	
}

int16_t adc_getsample(void){
	int16_t result;

	ADCA.CH0.CTRL |= ADC_CH_START_bm; // start conversion on channel 0
	
	while(!(ADCA.CH0.INTFLAGS & 0x0F));
	result = ADCA.CH0RES;
	ADCA.INTFLAGS = 0x0F;	//Clear flags
	return result;
}

//Do simultanoius conversions on 3 channels:
void adc_init_hall(void){
		PORTA.DIR = 0;	 // configure PORTA as input
		ADCA.CTRLA = 1;	
		ADCA.CTRLB = ADC_RESOLUTION_12BIT_gc;	 // 12 bit conversion
		ADCA.INTFLAGS = 0x0F;	//Clear flags		
		ADCA.REFCTRL = ADC_REFSEL_INT1V_gc;	 // internal 1V bandgap reference
		ADCA.PRESCALER = ADC_PRESCALER_DIV16_gc;	 // peripheral clk/8 (2MHz/16=250kHz)
		ADCA.CH0.CTRL = ADC_CH_INPUTMODE_SINGLEENDED_gc;	 // single ended
		ADCA.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN4_gc;
		
		ADCA.CH1.CTRL = ADC_CH_INPUTMODE_SINGLEENDED_gc;	 // single ended
		ADCA.CH1.MUXCTRL = ADC_CH_MUXPOS_PIN5_gc;
		
		ADCA.CH2.CTRL = ADC_CH_INPUTMODE_SINGLEENDED_gc;	 // single ended
		ADCA.CH2.MUXCTRL = ADC_CH_MUXPOS_PIN6_gc;
}

//Do simultanoius conversions on 3 channels:
void adc_get_hall(uint16_t* hall){
	//Start conversions	
	ADCA.CH0.CTRL |= ADC_CH_START_bm;
	ADCA.CH1.CTRL |= ADC_CH_START_bm;
	ADCA.CH2.CTRL |= ADC_CH_START_bm;

	
	while(!(ADCA.CH0.INTFLAGS & 0x0F));
	*hall++ = ADCA.CH0RES;
	while(!(ADCA.CH1.INTFLAGS & 0x0F));
	*hall++ = ADCA.CH1RES;
	while(!(ADCA.CH2.INTFLAGS & 0x0F));
	*hall++ = ADCA.CH2RES;
	
	
	ADCA.INTFLAGS = 0x0F;	//Clear flags
	
}

void adc_gesamples(int16_t* result){
	ADCA.CH0.CTRL |= ADC_CH_START_bm;
	
	while(!(ADCA.CH0.INTFLAGS & 0x0F));
	result[0] = ADCA.CH0RES;
	while(!(ADCA.CH1.INTFLAGS & 0x0F));
	result[1] = ADCA.CH1RES;
	ADCA.INTFLAGS = 0x0F;	//Clear flags	
}


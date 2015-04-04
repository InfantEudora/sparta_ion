/*
 * adc.h
 *
 * Created: 10/16/2013 2:10:49 PM
 *  Author: Dick
 */ 


#ifndef ADC_H_
#define ADC_H_

#include <stdint.h>
#include <avr/io.h>

void adc_init_single_ended(uint8_t ref, uint8_t channel);
void adc_init_differential(uint8_t ref, uint8_t pos_channel);
void adc_init_differential_gain(uint8_t ref,uint8_t gain, uint8_t pos_channel);
void adc_init_temperature(void);

uint16_t adc_getsample(void);

#endif /* ADC_H_ */
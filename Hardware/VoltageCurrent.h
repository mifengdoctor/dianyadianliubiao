#ifndef __VOLTAGE_CURRENT_H
#define __VOLTAGE_CURRENT_H

#include <stdint.h>

#define ADC_SAMPLE_SIZE 100U

/* DMA依次写入：电压通道PA2(ADC_IN2)、电流通道PA3(ADC_IN3)。 */
extern uint16_t Volt_Buffer[ADC_SAMPLE_SIZE];
extern uint16_t Curr_Buffer[ADC_SAMPLE_SIZE];

void VC_Init(void);
void VC_TimerSample(void);

#endif

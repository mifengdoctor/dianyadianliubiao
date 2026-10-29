#ifndef __CALIBRATION_H
#define __CALIBRATION_H

#include <stdint.h>

#define CAL_MODE_MEASURE       0U
#define CAL_MODE_VOLT_5V       1U
#define CAL_MODE_VOLT_15V      2U
#define CAL_MODE_CURR_05A      3U
#define CAL_MODE_CURR_15A      4U

void Calibration_Init(void);
void Calibration_Convert(uint16_t raw_voltage, uint16_t raw_current,
                         uint32_t *voltage_10mv, uint32_t *current_10ma);
uint8_t Calibration_SavePoint(uint8_t mode, uint16_t raw_voltage, uint16_t raw_current);

#endif

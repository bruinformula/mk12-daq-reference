/*
 * current_calculations.h
 *
 *  Created on: Feb 21, 2026
 *      Author: ishanchitale
 */

#ifndef INC_CURRENT_CALCULATIONS_H_
#define INC_CURRENT_CALCULATIONS_H_

#include <stdint.h>
#include <math.h>
#include "cmsis_os.h"
#include "safety_handler.h"

#define OVERCURRENT_THRESHOLD 200

typedef struct CURRENT_CONTEXT{
	uint16_t current_sensor_low_adc;
	uint16_t current_sensor_high_adc;
	float current_sensor_low;
	float current_sensor_high;
	float current_sensor_val;
} CURRENT_CONTEXT;
extern volatile CURRENT_CONTEXT current_context;

void calculateCurrent();

#endif /* INC_CURRENT_CALCULATIONS_H_ */

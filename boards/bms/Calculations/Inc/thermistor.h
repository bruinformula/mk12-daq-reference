/*
 * thermistor.h
 *
 *  Created on: Feb 10, 2026
 *      Author: ishanchitale
 */

#ifndef INC_THERMISTOR_H_
#define INC_THERMISTOR_H_

#include "adBms_Application.h"
#include "serialPrintResult.h"
#include "safety_handler.h"
#include "cmsis_os.h"
#include "freertos_handles.h"
#include <math.h>

#define OVER_TEMP_THRESHOLD 60
#define UNDER_TEMP_THRESHOLD 10

typedef struct TEMP_CONTEXT {
	float avg_cell_temp;
	float lowest_cell_temp;
	float highest_cell_temp;
	float temp_conversions[TOTAL_IC][CELLS_PER_IC];
	int num_valid_cell_temps;
} TEMP_CONTEXT;
extern volatile TEMP_CONTEXT temp_context;

float voltageToTemp(float V);
void computeAllTemps(uint8_t tIC, cell_asic *ic);

#endif /* INC_THERMISTOR_H_ */

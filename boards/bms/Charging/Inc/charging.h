/*
 * charging.h
 *
 *  Created on: Feb 17, 2026
 *      Author: ishanchitale
 */

#ifndef INC_CHARGING_H_
#define INC_CHARGING_H_

#include "j_plug.h"
#include "elcon_charger.h"
#include "fdcan.h"
#include "state_of_charge.h"
#include "voltage_calculations.h"
#include "thermistor.h"

typedef enum {
	CHG_IDLE = 0,
	CHG_WAITING,
	CHG_ACTIVE,
	CHG_COMPLETE,
	CHG_ELCON_FAULT
} CHARGING_STATE;
extern volatile CHARGING_STATE charging_state;

#define CHARGER_VOLTAGE 415
#define MIN_CHARGING_CURRENT 0.5
#define RAMP_CELL_VOLTAGE_CHARGING_THRESHOLD 4.08
#define MAX_CELL_VOLTAGE_CHARGING_THRESHOLD 4.15
#define SCALING_RANGE (MAX_CELL_VOLTAGE_CHARGING_THRESHOLD - RAMP_CELL_VOLTAGE_CHARGING_THRESHOLD)
#define CURRENT_SENSOR_EPSILON 1.0

void charging_loop();

#endif /* INC_CHARGING_H_ */

/*
 * j_plug.h
 *
 *  Created on: Feb 17, 2026
 *      Author: ishanchitale
 */

#ifndef INC_J_PLUG_H_
#define INC_J_PLUG_H_

#include <stdint.h>
#include <math.h>
#include <stdbool.h>
#include "adc.h"
#include "tim.h"
#include "cmsis_os.h"

#define PP_VOLTAGE_EPSILON 0.2
#define TIMER_CLOCK 1000000 // 1 Mhz
#define CP_TIMEOUT_MS 500
#define EVT_CP_UPDATE (1 << 1)

// J1772 CONTROL PILOT
typedef enum {
	STATE_CP_NOT_AVAILABLE,
	STATE_CP_FAULT,
	STATE_CP_PWM_PRESENT, // J1772 Connected, 1 Khz PWM Present; actual charging dependent on J1772_PILOT_SWITCH State
} STATE_CP;
extern volatile STATE_CP control_pilot_state;

// J1772 PROXIMITY PILOT
typedef enum {
	STATE_PP_UNKNOWN,
	STATE_PP_NOT_CONNECTED, // J1772 not connected
	STATE_PP_BUTTON_PRESSED, // J1772 latch button pressed, temporarily stops charging
	STATE_PP_CONNECTED // J1772 connected
} STATE_PP;
extern volatile STATE_PP proximity_pilot_state;

// TODO: Make clearer with duty cycle, frequency, voltage?
typedef struct J1772_CONTEXT {
	volatile uint32_t control_pilot_period;
	volatile uint32_t control_pilot_pulse;
	volatile uint16_t proximity_pilot_adc;
	volatile uint32_t last_cp_update_tick;
} J1772_CONTEXT;
extern volatile J1772_CONTEXT j1772_context;

#define WALL_V_AC 120
#define EFFICIENCY 0.91
#define MAX_PACK_V_DC 415
extern float advertised_amps_ac;
extern float advertised_amps_dc;

void startPWM_Capture();
void stopPWM_Capture();
bool isControlPilotTimedOut(uint32_t tick);
STATE_PP readProximityPilot(uint16_t adc);
STATE_CP readControlPilot(uint32_t period, uint32_t pulse);

#endif /* INC_J_PLUG_H_ */

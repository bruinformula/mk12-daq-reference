/*
 * bms_state.h
 *
 *  Created on: Apr 1, 2026
 *      Author: ishanchitale
 */

#ifndef INC_BMS_STATE_H_
#define INC_BMS_STATE_H_

#include "gpio.h"
#include "usart.h"
#include "charging.h"
#include "balancing.h"
#include "adBms_Application.h"
#include "prchg.h"
#include "curr_limiting.h"
#include "gui.h"
#include "can_datalogging.h"

// BMS Logging Configuration
#define BMS_DATALOGGING_DISABLED 0
#define BMS_DATALOGGING_ENABLED 1
#define BMS_JSON_DATALOGGING_MODE BMS_DATALOGGING_DISABLED // !!!!!!
#define BMS_CAN_DATALOGGING_MODE BMS_DATALOGGING_ENABLED // !!!!!!

// BMS Fault Handling Configuration
#define BMS_FAULT_DISABLED 0
#define BMS_FAULT_ENABLED 1
#define BMS_FAULT_OVERVOLTAGE BMS_FAULT_ENABLED
#define BMS_FAULT_UNDERVOLTAGE BMS_FAULT_ENABLED
#define BMS_FAULT_OVERTEMP BMS_FAULT_ENABLED
#define BMS_FAULT_UNDERTEMP BMS_FAULT_ENABLED
#define BMS_FAULT_OVERCURRENT BMS_FAULT_DISABLED
#define BMS_FAULT_IC_DISCONNECT BMS_FAULT_DISABLED

typedef enum {
	BMS_IDLE = 0,
	BMS_INTERNAL_FAULT,
	BMS_EXTERNAL_FAULT,
	BMS_WAIT_FOR_GUI,
	BMS_CHARGING,
	BMS_BALANCING,
	BMS_PRECHARGING,
	BMS_DRIVE,
} BMS_STATE;
extern volatile BMS_STATE bms_state;

void determine_operating_state();
void reset_operating_state(BMS_STATE prev_state);
void wakeup_tasks();
void change_baud_rate_500();
void change_baud_rate_250();

void enter_wait_for_gui_mode();
void enter_precharge_mode();
void enter_drive_mode();
void enter_charging_mode();
void enter_balancing_mode();

void exit_wait_for_gui_mode();
void exit_precharge_mode();
void exit_drive_mode();
void exit_charging_mode();
void exit_balancing_mode();


#endif /* INC_BMS_STATE_H_ */

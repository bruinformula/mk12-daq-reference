/*
 * elcon.h
 *
 *  Created on: Feb 11, 2026
 *      Author: ishanchitale
 */

#ifndef INC_ELCON_CHARGER_H_
#define INC_ELCON_CHARGER_H_

#include "bms_state.h"
#include "fdcan.h"
#include "cmsis_os.h"
#include "freertos_handles.h"
#include <stdbool.h>

#define ELCON_CHARGER_TX_ID 0x1806E5F4
#define ELCON_CHARGER_RX_ID 0x18FF50E5

// ELCON CHARGER DATAFRAMES
typedef union CHARGER_MSG_1806e5f4 {
	struct __attribute__((packed)) {
		uint8_t max_charging_voltage_msb; // 0.1 V/Byte
		uint8_t max_charging_voltage_lsb;
		uint8_t max_charging_current_msb; // 0.1 A/Byte
		uint8_t max_charging_current_lsb;
		uint8_t charging_control;	  // 0: Start Charging; 1: Stop Charging
		uint8_t reserved5;	// Reserved
		uint8_t reserved6;	// Reserved
		uint8_t reserved7;	// Reserved
	} data;
	uint8_t array[8];
} msg_1806E5F4;

typedef union {
    uint8_t raw;
    struct {
        uint8_t hardware_fail      : 1;
        uint8_t over_temp_protection    : 1;
        uint8_t input_voltage_fault  : 1;
        uint8_t starting_state_off  : 1;
        uint8_t communication_timeout : 1;
        uint8_t reserved     : 3;
    } bits;
} charger_status;

typedef union CHARGER_MSG_18ff50e5 {
	struct __attribute__((packed)) {
		uint16_t charger_output_voltage;	// 0.1 V/Byte
		uint16_t charger_output_current;	// 0.1 A/Byte
		charger_status status_flags;		// Status Flags
		uint8_t reserved5; 	// Reserved
		uint8_t reserved6;	// Reserved
		uint8_t reserved7;	// Reserved
	} data;
	uint8_t array[8];
} msg_18FF50E5;

typedef struct ELCON_CHARGER_CONTEXT {
	FDCAN_TxHeaderTypeDef TxHeader_1806E5F4;
	msg_1806E5F4 chgmsg_1806E5F4_DF;

	msg_18FF50E5 chgmsg_18FF50E5_DF;
} ELCON_CHARGER_CONTEXT;

extern ELCON_CHARGER_CONTEXT elcon_charger_context;

void configureChargeTxMsg();
void parseChargerBroadcast();
void sendChargerRequest(float max_charging_voltage, float max_charging_current, bool stop_charging);
bool chargerFaultDetected();
bool startingStateOffOnly();

#endif /* INC_ELCON_CHARGER_H_ */

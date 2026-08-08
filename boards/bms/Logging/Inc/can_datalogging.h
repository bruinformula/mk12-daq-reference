/*
 * datalogging.h
 *
 *  Created on: Apr 1, 2026
 *      Author: ishanchitale
 */

#ifndef INC_CAN_DATALOGGING_H_
#define INC_CAN_DATALOGGING_H_

#include "fdcan.h"
#include "state_of_charge.h"
#include "current_calculations.h"
#include "voltage_calculations.h"
#include "thermistor.h"
#include "freertos_handles.h"
#include "cmsis_os.h"

#define VOLTAGE_TX_ID 0x6B0
#define TEMP_TX_ID 0x6B1
#define SOC_CURR_PACK_TX_ID 0x6B2

typedef union VOLTAGE_DF {
	struct __attribute__((packed)) {
		uint16_t avg_cell_voltage;
		uint16_t lowest_cell_voltage;
		uint16_t highest_cell_voltage;
		uint8_t num_valid_voltages;
		uint8_t reserved7;
	} data;
	uint8_t array[8];
} VOLTAGE_DF;

typedef union TEMP_DF {
	struct __attribute__((packed)) {
		uint16_t avg_temp;
		uint16_t highest_temp;
		uint16_t lowest_temp;
		uint8_t num_valid_temps;
		uint8_t reserved7;
	} data;
	uint8_t array[8];
} TEMP_DF;

typedef union SOC_CURR_PACK_DF {
	struct __attribute__((packed)) {
		uint16_t soc;
		uint16_t curr;
		uint16_t pack_voltage;
		uint8_t reserved6;
		uint8_t reserved7;
	} data;
	uint8_t array[8];
} SOC_CURR_PACK_DF;

void configureTemp_TxMsg();
void configureVoltage_TxMsg();
void configureSoc_Curr_Pack_TxMsg();
void sendVoltage();
void sendTemp();
void sendSoc_Curr_Pack();

#endif /* INC_CAN_DATALOGGING_H_ */

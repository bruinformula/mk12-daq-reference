/*
 * datalogging.c
 *
 *  Created on: Apr 1, 2026
 *      Author: ishanchitale
 */

#include <can_datalogging.h>

static TEMP_DF temp_df;
static VOLTAGE_DF voltage_df;
static SOC_CURR_PACK_DF soc_curr_pack_df;

static FDCAN_TxHeaderTypeDef Temp_TxHeader;
static FDCAN_TxHeaderTypeDef Voltage_TxHeader;
static FDCAN_TxHeaderTypeDef Soc_Curr_Pack_TxHeader;

void configureVoltage_TxMsg() {
	configureFDCAN_TxMessage_STD(&Voltage_TxHeader, VOLTAGE_TX_ID);
}

void configureTemp_TxMsg() {
	configureFDCAN_TxMessage_STD(&Temp_TxHeader, TEMP_TX_ID);
}

void configureSoc_Curr_Pack_TxMsg() {
	configureFDCAN_TxMessage_STD(&Soc_Curr_Pack_TxHeader, SOC_CURR_PACK_TX_ID);
}

void sendVoltage() {
	// CRITICAL REGION
	osMutexWait(VOLTAGE_MUTEXHandle, osWaitForever);
	voltage_df.data.avg_cell_voltage = (voltage_context.avg_cell_voltage*100);
	voltage_df.data.highest_cell_voltage = (voltage_context.highest_cell_voltage*100);
	voltage_df.data.lowest_cell_voltage = (voltage_context.lowest_cell_voltage*100);
	voltage_df.data.num_valid_voltages = (voltage_context.num_valid_cell_voltages);
	osMutexRelease(VOLTAGE_MUTEXHandle);

	// CRITICAL REGION
	osMutexWait(CAN_MUTEXHandle, osWaitForever);
	HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &Voltage_TxHeader, voltage_df.array);
	osMutexRelease(CAN_MUTEXHandle);
}

void sendTemp() {
	// CRITICAL REGION
	osMutexWait(TEMP_MUTEXHandle, osWaitForever);
	temp_df.data.avg_temp = (temp_context.avg_cell_temp*100);
	temp_df.data.highest_temp = (temp_context.highest_cell_temp*100);
	temp_df.data.lowest_temp = (temp_context.lowest_cell_temp*100);
	temp_df.data.num_valid_temps = (temp_context.num_valid_cell_temps);
	osMutexRelease(TEMP_MUTEXHandle);

	// CRITICAL REGION
	osMutexWait(CAN_MUTEXHandle, osWaitForever);
	HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &Temp_TxHeader, temp_df.array);
	osMutexRelease(CAN_MUTEXHandle);
}

void sendSoc_Curr_Pack() {
	// Note; slight mix of old and new data are possible.
	soc_curr_pack_df.data.curr = (current_context.current_sensor_val*100);
	soc_curr_pack_df.data.soc = (soc*100);
	soc_curr_pack_df.data.pack_voltage = (voltage_context.estimated_pack_voltage*100);

	// CRITICAL REGION
	osMutexWait(CAN_MUTEXHandle, osWaitForever);
	HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &Soc_Curr_Pack_TxHeader, soc_curr_pack_df.array);
	osMutexRelease(CAN_MUTEXHandle);
}

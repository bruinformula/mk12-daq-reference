/*
 * elcon.c
 *
 *  Created on: Feb 11, 2026
 *      Author: ishanchitale
 */

#include "elcon_charger.h"

ELCON_CHARGER_CONTEXT elcon_charger_context;

void configureChargeTxMsg() {
	configureFDCAN_TxMessage_EXTD(&elcon_charger_context.TxHeader_1806E5F4, ELCON_CHARGER_TX_ID);
}

void parseChargerBroadcast() {
	if (bms_state != BMS_CHARGING) return;

	// TODO: A bit unclear, should just put the 0.1 scaling into software?
	elcon_charger_context.chgmsg_18FF50E5_DF.data.charger_output_voltage = ((BMS_RxData[0] << 8) | BMS_RxData[1]);
	elcon_charger_context.chgmsg_18FF50E5_DF.data.charger_output_current = ((BMS_RxData[2] << 8) | BMS_RxData[3]);
	elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.raw = BMS_RxData[4];
}

void sendChargerRequest(float max_charging_voltage, float max_charging_current, bool stop_charging) {
	// To clarify: stop_charging = 0 ==> Keep Charging, stop_charging = 1 ==> STOP CHARGING!!!
	uint16_t voltage_raw = (uint16_t)(max_charging_voltage*10);
	uint16_t current_raw = (uint16_t)(max_charging_current*10);

	elcon_charger_context.chgmsg_1806E5F4_DF.data.max_charging_voltage_msb = (voltage_raw >> 8) & 0xFF;
	elcon_charger_context.chgmsg_1806E5F4_DF.data.max_charging_voltage_lsb = voltage_raw  & 0xFF;
	elcon_charger_context.chgmsg_1806E5F4_DF.data.max_charging_current_msb = (current_raw >> 8) & 0xFF;
	elcon_charger_context.chgmsg_1806E5F4_DF.data.max_charging_current_lsb = current_raw & 0xFF;
	elcon_charger_context.chgmsg_1806E5F4_DF.data.charging_control = stop_charging;

	// CRITICAL REGION
	osMutexWait(CAN_MUTEXHandle, osWaitForever);
	HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &elcon_charger_context.TxHeader_1806E5F4, elcon_charger_context.chgmsg_1806E5F4_DF.array);
	osMutexRelease(CAN_MUTEXHandle);
}

bool chargerFaultDetected() {
    return (
        elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.bits.communication_timeout ||
        elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.bits.hardware_fail ||
        elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.bits.input_voltage_fault ||
        elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.bits.over_temp_protection ||
        elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.bits.starting_state_off
    );
}

bool startingStateOffOnly() {
    return (
        !elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.bits.communication_timeout &&
        !elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.bits.hardware_fail &&
        !elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.bits.input_voltage_fault &&
        !elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.bits.over_temp_protection &&
        elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.bits.starting_state_off
    );
}

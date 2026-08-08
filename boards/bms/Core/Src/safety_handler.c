/*
 * safety_handler.c
 *
 *  Created on: Apr 1, 2026
 *      Author: ishanchitale
 */

#include "safety_handler.h"

static FDCAN_TxHeaderTypeDef Shutdown_Lost_TxHeader;
static uint8_t Shutdown_Lost_TxData[8];

void configureShutdownLostTxMsg() {
	configureFDCAN_TxMessage_STD(&Shutdown_Lost_TxHeader, BMS_SHUTDOWN_LOST_TX_ID);
}

void service_shutdown_power_signal(void)
{
    static GPIO_PinState last_sample = GPIO_PIN_RESET;
    static GPIO_PinState debounced_state = GPIO_PIN_RESET;

    static uint32_t last_change_time = 0;

    GPIO_PinState sample = HAL_GPIO_ReadPin(SHUTDOWN_POWER_GPIO_Port, SHUTDOWN_POWER_Pin);

    uint32_t now = HAL_GetTick();

    if (sample != last_sample) {
        last_sample = sample;
        last_change_time = now;
    }

    if ((now - last_change_time) < 100U) {
        return;
    }

    if (sample != debounced_state) {
        debounced_state = sample;
        bool shutdown_asserted =
            (sample == GPIO_PIN_SET);
        apply_shutdown_power_state(shutdown_asserted);
    }
}

static volatile bool bms_fault_issued = false;
void apply_shutdown_power_state(bool shutdown_asserted) {
	if (shutdown_asserted) {
		if (bms_state == BMS_IDLE || bms_state == BMS_INTERNAL_FAULT || bms_state == BMS_EXTERNAL_FAULT) {
			bms_fault_issued = false; // Clamp back to false.
			bms_state = BMS_IDLE;
			determine_operating_state();
		}
	} else {
		// OPEN AIRs!
		HAL_GPIO_WritePin(POS_AIR_GND_GPIO_Port, POS_AIR_GND_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(NEG_AIR_GND_GPIO_Port, NEG_AIR_GND_Pin, GPIO_PIN_RESET);
		// NOTE: Precharge Relay would only ever be closed for precharging.
		// Still, we assert that it is open.
		HAL_GPIO_WritePin(PRECHARGE_GPIO_Port, PRECHARGE_Pin, GPIO_PIN_RESET);

		reset_operating_state(bms_state);
		if (bms_fault_issued) {
			bms_state = BMS_INTERNAL_FAULT;
		} else {
			bms_state = BMS_EXTERNAL_FAULT;
		}

		osMutexWait(CAN_MUTEXHandle, osWaitForever);
		HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &Shutdown_Lost_TxHeader, Shutdown_Lost_TxData);
		osMutexRelease(CAN_MUTEXHandle);
	}
}

volatile BMS_FaultRegister fault_register;

void BMS_SetFault(uint8_t fault) {
	// CRITICAL REGION
	osMutexWait(FAULT_MUTEXHandle, osWaitForever);
	fault_register.reg |= fault;
    osMutexRelease(FAULT_MUTEXHandle);
}

void BMS_ClearFault(uint8_t fault) {
	// CRITICAL REGION
	osMutexWait(FAULT_MUTEXHandle, osWaitForever);
    fault_register.reg &= ~fault;
    osMutexRelease(FAULT_MUTEXHandle);
}

uint8_t BMS_GetFaultRegister() {
	uint8_t snapshot;
	// CRITICAL REGION
	osMutexWait(FAULT_MUTEXHandle, osWaitForever);
	snapshot = fault_register.reg;
	osMutexRelease(FAULT_MUTEXHandle);
	return snapshot;
}

void BMS_CheckFaultRegister() {
	// CRITICAL REGION
	osMutexWait(FAULT_MUTEXHandle, osWaitForever);
	if (fault_register.reg != 0) {
		// Issue BMS Fault!
		bms_fault_issued = true;
		HAL_GPIO_WritePin(BMS_FAULT_GPIO_Port, BMS_FAULT_Pin, GPIO_PIN_RESET);
	} else {
		// Shutdown Reset needs to be hit for fault to clear on Shutdown Board.
		// Even if the pin is driven high (NO FAULT) on the BMS.
		HAL_GPIO_WritePin(BMS_FAULT_GPIO_Port, BMS_FAULT_Pin, GPIO_PIN_SET);
	}
	osMutexRelease(FAULT_MUTEXHandle);
}

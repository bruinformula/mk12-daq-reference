/*
 * prchg.h
 *
 *  Created on: Feb 11, 2026
 *      Author: ishanchitale
 */

#ifndef INC_PRCHG_H_
#define INC_PRCHG_H_

#include <math.h>
#include <stdbool.h>
#include "voltage_calculations.h"
#include "gpio.h"
#include "tim.h"
#include "fdcan.h"
#include "adBms_Application.h"
#include "serialPrintResult.h"
#include "freertos_handles.h"
#include "cmsis_os.h"
#include "bms_state.h"

#define PRECHARGE_IGNORE_TX_ID 0x6B5
#define PRECHARGE_COMPLETE_TX_ID 0x6B4
#define PRECHARGE_REQUEST_RX_ID 0x6B3
#define INVERTER_VOLTAGE_RX_ID 0xA7
#define PRECHARGE_VOLTAGE_DELTA 25
#define PRECHARGE_TIMEOUT_MS 5000

typedef union PRECHARGE_COMPLETE_DF {
	struct __attribute__((packed)) {
		uint8_t inverter_precharged;
		uint8_t reserved0;
		uint8_t reserved1;
		uint8_t reserved2;
		uint8_t reserved3;
		uint8_t reserved4;
		uint8_t reserved5;
		uint8_t reserved6;
	} data;
	uint8_t array[8];
} PRECHARGE_COMPLETE_DF;

typedef enum {
    PRECHARGE_IDLE,
    PRECHARGE_ACTIVE,
    PRECHARGE_SUCCESS,
	PRECHARGE_FAIL,
} PRECHARGE_STATE;
extern volatile PRECHARGE_STATE precharge_state;

extern volatile float inverter_dc_volts;

void configurePrchgIgnoreTxMsg();
void configurePrchgTxMsg();
void prechargeStart();
void precharge_loop();

#endif /* INC_PRCHG_H_ */

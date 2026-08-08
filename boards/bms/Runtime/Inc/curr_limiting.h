/*
 * currLimiting.h
 *
 *  Created on: Mar 31, 2026
 *      Author: ishanchitale
 */

#ifndef INC_CURR_LIMITING_H_
#define INC_CURR_LIMITING_H_

#include "thermistor.h"
#include "fdcan.h"
#include "freertos_handles.h"
#include "cmsis_os.h"

#define DCL_CCL_TX_ID 0x202
#define DCL_CURVE_POINTS 17
#define CCL_CURVE_POINTS 13

typedef union DCL_CCL_DF {
	struct __attribute__((packed)) {
		uint16_t pack_dcl;
		uint16_t pack_ccl;
		uint8_t reserved4;
		uint8_t reserved5;
		uint8_t reserved6;
		uint8_t reserved7;
	} data;
	uint8_t array[8];
} DCL_CCL_DF;

typedef struct CurvePoint {
	float temp;
	float current;
} CurvePoint;

extern float dcl;
extern float ccl;

void calculateDCL(float cell_temp);
void calculateCCL(float cell_temp);
void configureDCL_CCL_TxMsg();
void sendDCL_CCL();

#endif /* INC_CURR_LIMITING_H_ */

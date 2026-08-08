/*
 * currLimiting.c
 *
 *  Created on: Mar 31, 2026
 *      Author: ishanchitale
 */

#include "curr_limiting.h"

float dcl;
float ccl;
static DCL_CCL_DF dcl_ccl_df;
static FDCAN_TxHeaderTypeDef DCL_CCL_TxHeader;

static float interpolateCurve(const CurvePoint *curve, int size, float x) {
    // Clamp below range
    if (x <= curve[0].temp)
        return curve[0].current;

    // Clamp above range
    if (x >= curve[size - 1].temp)
        return curve[size - 1].current;

    // Find segment
    for (int i = 0; i < size - 1; i++) {
        if (x < curve[i + 1].temp) {
            float x0 = curve[i].temp;
            float y0 = curve[i].current;
            float x1 = curve[i + 1].temp;
            float y1 = curve[i + 1].current;

            // Linear interpolation
            return y0 + (x - x0) * (y1 - y0) / (x1 - x0);
        }
    }

    return 0.0f; // fallback (should never hit)
}

static const CurvePoint CCL_Curve[CCL_CURVE_POINTS] = {
	{-20.0f, 0.0f},
	{-10.0f, 0.0f},
	{  0.0f, 0.0f},
	{  5.0f, 5.0f},
	{ 10.0f, 15.0f},
	{ 15.0f, 25.0f},
	{ 20.0f, 30.0f},
	{ 25.0f, 30.0f},
	{ 35.0f, 30.0f},
	{ 40.0f, 20.0f},
	{ 45.0f, 5.0f},
	{ 50.0f, 0.0f},
	{ 60.0f, 0.0f}
};

static const CurvePoint DCL_Curve[DCL_CURVE_POINTS] = {
	{-20.0f, 0.0f},
	{-10.0f, 40.0f},
	{  0.0f, 100.0f},
	{ 10.0f, 160.0f},
	{ 15.0f, 180.0f},
	{ 25.0f, 180.0f},
	{ 35.0f, 180.0f},
	{ 45.0f, 180.0f},
	{ 50.0f, 180.0f},
	{ 55.0f, 180.0f},
	{ 60.0f, 180.0f},
	{ 65.0f, 180.0f},
	{ 70.0f, 180.0f},
	{ 75.0f, 180.0f},
	{ 80.0f, 180.0f},
	{ 90.0f, 100.0f},
	{ 100.0f, 0.0f},
};

void calculateDCL(float cell_temp) {
	dcl = interpolateCurve(DCL_Curve, DCL_CURVE_POINTS, cell_temp);
}

void calculateCCL(float cell_temp) {
	ccl = interpolateCurve(CCL_Curve, CCL_CURVE_POINTS, cell_temp);
}

void configureDCL_CCL_TxMsg() {
	configureFDCAN_TxMessage_STD(&DCL_CCL_TxHeader, DCL_CCL_TX_ID);
}

void sendDCL_CCL() {
	if (bms_state != BMS_DRIVE) return;

	calculateDCL(temp_context.highest_cell_temp);
	calculateCCL(temp_context.lowest_cell_temp);
	dcl_ccl_df.data.pack_dcl = (uint16_t)(dcl*100);
	dcl_ccl_df.data.pack_ccl = (uint16_t)(ccl*100);

	osMutexWait(CAN_MUTEXHandle, osWaitForever);
	HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &DCL_CCL_TxHeader, dcl_ccl_df.array);
	osMutexRelease(CAN_MUTEXHandle);
}

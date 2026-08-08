/*
 * thermistor.c
 *
 *  Created on: Feb 10, 2026
 *      Author: ishanchitale
 */

#include "thermistor.h"

volatile TEMP_CONTEXT temp_context;
static float local_temp_conversions[TOTAL_IC][CELLS_PER_IC];

static const float voltage_table[33] = {
		2.44, 2.42, 2.40, 2.38, 2.35, 2.32, 2.27, 2.23, 2.17, 2.11, 2.05, 1.99, 1.92, 1.86, 1.8, 1.74, 1.68,
		1.63, 1.59, 1.55, 1.51, 1.48, 1.45, 1.43, 1.40, 1.38, 1.37, 1.35, 1.34, 1.33, 1.32, 1.31, 1.30
};

static const float temp_table[33] = {
		-40, -35, -30, -25, -20, -15, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50,
		55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120
};

float voltageToTemp(float V) {
	if (V > voltage_table[0] || V < voltage_table[32]) {
		return NAN; // OUT OF RANGE
	}

	for (size_t i = 0; i < 32; i++) {
		float v_high = voltage_table[i];      // Higher voltage, lower temp
		float v_low = voltage_table[i + 1];   // Lower voltage, higher temp

		if (V <= v_high && V >= v_low) {
			float t_high = temp_table[i];
			float t_low = temp_table[i + 1];

			// LINEAR INTERPOLATION
			float temp = t_high + (V - v_high) * (t_low - t_high) / (v_low - v_high);
			return temp;
		}
	}

	return NAN; // SHOULD NOT REACH HERE!
}

void computeAllTemps(uint8_t tIC, cell_asic *ic) {
    float local_lowest = INFINITY;
    float local_highest = -INFINITY;
    float local_avg = 0.0f;
    float tempSum = 0.0f;
    int local_valid_cells = 0;
    bool any_ic_disconnect = false;

	osMutexWait(SPI_MUTEXHandle, osWaitForever);
	adBms6830_read_aux_voltages(tIC, ic);
	osMutexRelease(SPI_MUTEXHandle);

	for (size_t i = 0; i < tIC; ++i) {
		int potential_invalid_codes = 0;

		// FIRST PASS: Scan for IC Disconnect.
		for (size_t j = 0; j < CELLS_PER_IC; ++j) {
			if (ic[i].aux.a_codes[j] == -1) {
				potential_invalid_codes++;
			}
		}

		if (potential_invalid_codes == CELLS_PER_IC) {
			any_ic_disconnect = true;
			for (size_t j = 0; j < CELLS_PER_IC; ++j) {
				local_temp_conversions[i][j] = NAN;
			}
			continue;
		}

		// SECOND PASS: IC is connected.
		for (size_t j = 0; j < CELLS_PER_IC; ++j) {
			float cell_temp = voltageToTemp(getVoltage(ic[i].aux.a_codes[j]));
			local_temp_conversions[i][j] = cell_temp;

			if (isnan(cell_temp)) {
				continue;
			}

			local_valid_cells++;

			if (cell_temp < local_lowest) {
				local_lowest = cell_temp;
			}

			if (cell_temp > local_highest) {
				local_highest = cell_temp;
			}

            tempSum += cell_temp;
		}
	}

	if (local_valid_cells > 0) {
		local_avg = tempSum/(local_valid_cells);
	} else {
		local_avg = NAN;
	}

	// CRITICAL REGION
	osMutexWait(TEMP_MUTEXHandle, osWaitForever);
	temp_context.num_valid_cell_temps = local_valid_cells;
	temp_context.lowest_cell_temp = local_lowest;
	temp_context.highest_cell_temp = local_highest;
	temp_context.avg_cell_temp = local_avg;
	memcpy(temp_context.temp_conversions, local_temp_conversions, sizeof(local_temp_conversions));
	osMutexRelease(TEMP_MUTEXHandle);

	// FAULT HANDLING
	uint8_t faults_set = 0;
	uint8_t faults_clear = 0;

#if BMS_FAULT_IC_DISCONNECT == BMS_FAULT_ENABLED
	if (any_ic_disconnect) {
		faults_set |= FAULT_ISOSPI_DISCONNECT;
	} else {
		faults_clear |= FAULT_ISOSPI_DISCONNECT;
	}
#endif

#if BMS_FAULT_OVERTEMP == BMS_FAULT_ENABLED
	if (local_highest > OVER_TEMP_THRESHOLD) {
		faults_set |= FAULT_OVERTEMP;
	} else {
		faults_clear |= FAULT_OVERTEMP;
	}
#endif

#if BMS_FAULT_UNDERTEMP == BMS_FAULT_ENABLED
	if (local_lowest < UNDER_TEMP_THRESHOLD) {
		faults_set |= FAULT_UNDERTEMP;
	} else {
		faults_clear |= FAULT_UNDERTEMP;
	}
#endif

	if (faults_set) {
		BMS_SetFault(faults_set);
	}
	if (faults_clear) {
		BMS_ClearFault(faults_clear);
	}
}

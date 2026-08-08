#include "voltage_calculations.h"

volatile VOLTAGE_CONTEXT voltage_context;
static float local_voltage_conversions[TOTAL_IC][CELLS_PER_IC];

void computeAllVoltages(uint8_t tIC, cell_asic *ic) {
    float local_lowest = INFINITY;
    float local_highest = -INFINITY;
    float local_avg = 0.0f;
    float local_estimated_pack = 0.0f;
    int local_valid_cells = TOTAL_CELLS;

	osMutexWait(SPI_MUTEXHandle, osWaitForever);
	adBms6830_read_cell_voltages(tIC, ic);
	osMutexRelease(SPI_MUTEXHandle);

    float measured_pack = 0.0f;
	for (size_t i = 0; i < tIC; ++i) {
		for (size_t j = 0; j < CELLS_PER_IC; ++j) {
			float cell_voltage = getVoltage(ic[i].cell.c_codes[j]);
			local_voltage_conversions[i][j] = cell_voltage;

			if (cell_voltage <= BROKEN_CELL_VOLTAGE_THRESHOLD) {
				local_valid_cells--;
				continue;
			}

			if (cell_voltage < local_lowest) {
				local_lowest = cell_voltage;
			}

			if (cell_voltage > local_highest) {
				local_highest = cell_voltage;
			}

			measured_pack += cell_voltage;
		}
	}

	if (local_valid_cells > 0) {
		local_avg = measured_pack/local_valid_cells;
		local_estimated_pack = measured_pack + local_avg*(TOTAL_CELLS - local_valid_cells);
	} else {
		local_avg = NAN;
		local_estimated_pack = NAN;
	}

	// CRITICAL REGION
	osMutexWait(VOLTAGE_MUTEXHandle, osWaitForever);
	voltage_context.num_valid_cell_voltages = local_valid_cells;
	voltage_context.lowest_cell_voltage = local_lowest;
	voltage_context.highest_cell_voltage = local_highest;
	voltage_context.avg_cell_voltage = local_avg;
	voltage_context.estimated_pack_voltage = local_estimated_pack;
	memcpy(voltage_context.voltage_conversions, local_voltage_conversions, sizeof(local_voltage_conversions));
    osMutexRelease(VOLTAGE_MUTEXHandle);

    // FAULT HANDLING
	uint8_t faults_set = 0;
	uint8_t faults_clear = 0;

#if BMS_FAULT_OVERVOLTAGE == BMS_FAULT_ENABLED
	if (local_highest > OVER_VOLTAGE_THRESHOLD) {
		faults_set |= FAULT_OVERVOLTAGE;
	} else {
		faults_clear |= FAULT_OVERVOLTAGE;
	}
#endif

#if BMS_FAULT_UNDERVOLTAGE == BMS_FAULT_ENABLED
	// Assert this is a real under-voltage fault, NOT an ISOSPI Disconnect in which cell voltages read ~1.5V.
	// ISOSPI Disconnect fault detection is handled in thermistor.c.
	if (local_lowest < UNDER_VOLTAGE_THRESHOLD && local_lowest > BROKEN_CELL_VOLTAGE_THRESHOLD) {
		faults_set |= FAULT_UNDERVOLTAGE;
	} else {
		faults_clear |= FAULT_UNDERVOLTAGE;
	}
#endif

    if (faults_set) {
    	BMS_SetFault(faults_set);
    }
    if (faults_clear) {
    	BMS_ClearFault(faults_clear);
    }
}

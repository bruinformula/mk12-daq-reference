#include "state_of_charge.h"

static float interpolate(float x0, float x1, float y0, float y1, float x) {
    if (x0 == x1) return y0;
    return y0 + (x - x0) * (y1 - y0) / (x1 - x0);
}

static float ocv_lookup_soc(float avg_temp, float cell_voltage) {
	(void)avg_temp;

    for (size_t k = 0; k < NUM_SOC - 1; ++k) {

        float v1 = ocv_table[0][k];
        float v2 = ocv_table[0][k + 1];

        float min_v = fminf(v1, v2);
        float max_v = fmaxf(v1, v2);

        if (cell_voltage >= min_v && cell_voltage <= max_v) {

            return interpolate(
                v1, v2,
                soc_axis[k], soc_axis[k + 1],
                cell_voltage
            );
        }
    }

    return NAN;

}

float soc;

void get_initial_soc() {
	float temp;
	float v_high;
	float v_low;

	// CRITICAL REGION
	osMutexWait(VOLTAGE_MUTEXHandle, osWaitForever);
	osMutexWait(TEMP_MUTEXHandle, osWaitForever);
	temp = temp_context.avg_cell_temp;
	v_low = voltage_context.lowest_cell_voltage;
	v_high = voltage_context.highest_cell_voltage;
	osMutexRelease(TEMP_MUTEXHandle);
	osMutexRelease(VOLTAGE_MUTEXHandle);

	float soc_min = ocv_lookup_soc(temp, v_low);
	float soc_max = ocv_lookup_soc(temp, v_high);

	if (isnan(soc_min) || isnan(soc_max)) {
		soc = NAN;
	} else {
		if (soc_min <= 50.0f) {
			soc = soc_min;
		} else {
			soc = soc_max;
		}
	}
}

void coulomb_count(uint32_t dt_ms) {
	if (isnan(soc)) {
		return;
	}

	float dt_seconds = (float)dt_ms / 1000.0f;
	float delta_soc = (current_context.current_sensor_val * dt_seconds)/NOMINAL_PACK_CAPACITY_AS;
	delta_soc *= 100.0f;
	soc -= delta_soc;

	if (soc > 100.0f) {
		soc = 100.0f;
	} else if (soc < 0.0f) {
		soc = 0.0f;
	}
}

/*
 * gui_test.c
 *
 *  Created on: Apr 4, 2026
 *      Author: ishanchitale
 */

#include <gui.h>
#include "bms_state.h"
#include "balancing.h"
#include "elcon_charger.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

uint8_t uart_rx_buffer[RX_BUF_SIZE];
char serial_cmd_buffer[RX_BUF_SIZE];

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t len) {
	if (huart->Instance == LPUART1) {
		uint16_t copy_len = (len < RX_BUF_SIZE - 1) ? len : (RX_BUF_SIZE - 1);
		memcpy(serial_cmd_buffer, uart_rx_buffer, copy_len);
	    serial_cmd_buffer[copy_len] = '\0';

	    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	    xTaskNotifyFromISR(serialCmdTaskHandle, 0, eIncrement, &xHigherPriorityTaskWoken);

	    HAL_UARTEx_ReceiveToIdle_DMA(&hlpuart1, uart_rx_buffer, RX_BUF_SIZE);
	    __HAL_DMA_DISABLE_IT(hlpuart1.hdmarx, DMA_IT_HT);

	    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

void return_to_gui_from_charging_mode() {
	charging_state = CHG_IDLE;

	HAL_GPIO_WritePin(J1772_PILOT_SWITCH_GPIO_Port, J1772_PILOT_SWITCH_Pin, GPIO_PIN_RESET);
	sendChargerRequest(0, 0, 1);

	// change_baud_rate_500();
	// Keep Baud Rate at 250 Kbps for debugging reasons!
	// Still want to use CAN even if we are not actively charging.
	stopPWM_Capture();
	// Control Pilot readings NOT necessary if charging is not active.

	// OPEN AIRs!
	// NOTE: exit_charging_mode() is called upon loss of shutdown power
	// So, AIRs and Precharge Relay are opened from the general shutdown power loss handler.
	// Here, we need to explicitly put it into return_to_gui_from_charging_mode().
	HAL_GPIO_WritePin(POS_AIR_GND_GPIO_Port, POS_AIR_GND_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(NEG_AIR_GND_GPIO_Port, NEG_AIR_GND_Pin, GPIO_PIN_RESET);

	enter_wait_for_gui_mode();
}

void return_to_gui_from_balancing_mode() {
	balance_state = BALANCE_IDLE;
	balance_percent = 0;
	balance_pwm = 0;
	num_unbalanced_cells = 0;

	// Reset Discharge Current %, DCTO, and DCC bits.
	for (size_t i = 0; i < TOTAL_IC; ++i) {
		memset(IC[i].PwmA.pwma, 0x00, sizeof(IC[i].PwmA.pwma));
		memset(IC[i].PwmB.pwmb, 0x00, sizeof(IC[i].PwmB.pwmb));
		IC[i].tx_cfgb.dcc = 0;
		IC[i].tx_cfgb.dcto = 0;
	}

	osMutexWait(SPI_MUTEXHandle, osWaitForever);
	adBms6830_write_config(TOTAL_IC, IC);
	adBmsWriteData(TOTAL_IC, IC, WRPWM1, Pwm, A);
	adBmsWriteData(TOTAL_IC, IC, WRPWM2, Pwm, B);
	osMutexRelease(SPI_MUTEXHandle);

	enter_wait_for_gui_mode();
}

void processGUI_Cmd() {
	char *newline = strchr(serial_cmd_buffer, '\n');
	if (newline) *newline = '\0';

	char *carriage = strchr(serial_cmd_buffer, '\r');
	if (carriage) *carriage = '\0';

	if (bms_state == BMS_WAIT_FOR_GUI) {
		if (strcmp(serial_cmd_buffer, "ENTER_CHG_MODE") == 0) {
			enter_charging_mode();
	    } else if (strcmp(serial_cmd_buffer, "ENTER_BAL_MODE") == 0) {
	    	enter_balancing_mode();
	    }
	} else if (bms_state == BMS_CHARGING) {
		if (strcmp(serial_cmd_buffer, "EXIT_CHG_MODE") == 0) {
			return_to_gui_from_charging_mode();
		}
	} else if (bms_state == BMS_BALANCING) {
		if (strcmp(serial_cmd_buffer, "EXIT_BAL_MODE") == 0) {
			return_to_gui_from_balancing_mode();
		} else if (strncmp(serial_cmd_buffer, "START_BAL", 9) == 0) {
			int requested_percent = atoi(&serial_cmd_buffer[10]);
			startBalancingLoop(requested_percent);
		}
	}
}

char json_buf[4096];

static const char *bms_state_to_string(BMS_STATE state) {
	switch (state) {
		case BMS_IDLE: return "idle";
		case BMS_INTERNAL_FAULT: return "internal_fault";
		case BMS_EXTERNAL_FAULT: return "external_fault";
		case BMS_WAIT_FOR_GUI: return "wait_for_gui";
		case BMS_CHARGING: return "charging";
		case BMS_BALANCING: return "balancing";
		case BMS_PRECHARGING: return "precharging";
		case BMS_DRIVE: return "drive";
		default: return "unknown";
	}
}

static const char *charging_state_to_string(CHARGING_STATE state) {
	switch (state) {
		case CHG_IDLE: return "idle";
		case CHG_WAITING: return "waiting";
		case CHG_ACTIVE: return "active";
		case CHG_COMPLETE: return "complete";
		case CHG_ELCON_FAULT: return "elcon_fault";
		default: return "unknown";
	}
}

static const char *balance_state_to_string(BalanceState state) {
	switch (state) {
		case BALANCE_IDLE: return "idle";
		case BALANCE_COMPUTE_DISCHARGE: return "compute_discharge";
		case BALANCE_DISCHARGE: return "discharge";
		case BALANCE_WAIT: return "wait";
		case BALANCE_COMPLETE: return "complete";
		default: return "unknown";
	}
}

static const char *precharge_state_to_string(PRECHARGE_STATE state) {
	switch (state) {
		case PRECHARGE_IDLE: return "idle";
		case PRECHARGE_ACTIVE: return "active";
		case PRECHARGE_SUCCESS: return "success";
		case PRECHARGE_FAIL: return "fail";
		default: return "unknown";
	}
}

static const char *control_pilot_state_to_string(STATE_CP state) {
	switch (state) {
		case STATE_CP_NOT_AVAILABLE: return "not_available";
		case STATE_CP_FAULT: return "fault";
		case STATE_CP_PWM_PRESENT: return "pwm_present";
		default: return "unknown";
	}
}

static const char *proximity_pilot_state_to_string(STATE_PP state) {
	switch (state) {
		case STATE_PP_UNKNOWN: return "unknown";
		case STATE_PP_NOT_CONNECTED: return "not_connected";
		case STATE_PP_BUTTON_PRESSED: return "button_pressed";
		case STATE_PP_CONNECTED: return "connected";
		default: return "unknown";
	}
}

static int append_json(char **cursor, size_t *remaining, const char *format, ...) {
	if (*remaining == 0) {
		return -1;
	}

	va_list args;
	va_start(args, format);
	int written = vsnprintf(*cursor, *remaining, format, args);
	va_end(args);

	if (written < 0 || (size_t)written >= *remaining) {
		*remaining = 0;
		return -1;
	}

	*cursor += written;
	*remaining -= (size_t)written;
	return 0;
}

static int append_json_scaled_int(char **cursor, size_t *remaining, float value, uint32_t scale) {
	if (!isfinite(value)) {
		return append_json(cursor, remaining, "null");
	}

	float scaled_value = value * (float)scale;
	if (scaled_value > (float)INT32_MAX || scaled_value < (float)INT32_MIN) {
		return append_json(cursor, remaining, "null");
	}

	int32_t scaled = (int32_t)(scaled_value + ((scaled_value >= 0.0f) ? 0.5f : -0.5f));
	return append_json(cursor, remaining, "%ld", (long)scaled);
}

static int append_scaled_matrix(char **cursor, size_t *remaining, const char *name,
		volatile float values[TOTAL_IC][CELLS_PER_IC]) {
	if (append_json(cursor, remaining, "\"%s\":[", name) != 0) return -1;
	for (size_t ic = 0; ic < TOTAL_IC; ++ic) {
		if (append_json(cursor, remaining, "%s[", (ic == 0) ? "" : ",") != 0) return -1;
		for (size_t cell = 0; cell < CELLS_PER_IC; ++cell) {
			if (cell > 0 && append_json(cursor, remaining, ",") != 0) return -1;
			if (append_json_scaled_int(cursor, remaining, values[ic][cell], 100U) != 0) return -1;
		}
		if (append_json(cursor, remaining, "]") != 0) return -1;
	}
	return append_json(cursor, remaining, "]");
}

int build_bms_json() {
	char *cursor = json_buf;
	size_t remaining = sizeof(json_buf);

	if (append_json(&cursor, &remaining,
			"{\"uptime_ms\":%lu,\"bms_state\":\"%s\",\"safety_register\":%u,"
			"\"safety\":{\"overvoltage\":%s,\"undervoltage\":%s,\"overtemp\":%s,"
			"\"undertemp\":%s,\"overcurrent\":%s,\"isospi_disconnect\":%s},",
			(unsigned long)HAL_GetTick(), bms_state_to_string(bms_state), fault_register.reg,
			(fault_register.reg & FAULT_OVERVOLTAGE) ? "true" : "false",
			(fault_register.reg & FAULT_UNDERVOLTAGE) ? "true" : "false",
			(fault_register.reg & FAULT_OVERTEMP) ? "true" : "false",
			(fault_register.reg & FAULT_UNDERTEMP) ? "true" : "false",
			(fault_register.reg & FAULT_OVERCURRENT) ? "true" : "false",
			(fault_register.reg & FAULT_ISOSPI_DISCONNECT) ? "true" : "false") != 0) return -1;

	if (append_json(&cursor, &remaining, "\"voltage\":{") != 0) return -1;
	if (append_json(&cursor, &remaining, "\"estimated_pack_voltage_x100\":") != 0) return -1;
	if (append_json_scaled_int(&cursor, &remaining, voltage_context.estimated_pack_voltage, 100U) != 0) return -1;
	if (append_json(&cursor, &remaining, ",\"average_cell_voltage_x100\":") != 0) return -1;
	if (append_json_scaled_int(&cursor, &remaining, voltage_context.avg_cell_voltage, 100U) != 0) return -1;
	if (append_json(&cursor, &remaining, ",\"lowest_cell_voltage_x100\":") != 0) return -1;
	if (append_json_scaled_int(&cursor, &remaining, voltage_context.lowest_cell_voltage, 100U) != 0) return -1;
	if (append_json(&cursor, &remaining, ",\"highest_cell_voltage_x100\":") != 0) return -1;
	if (append_json_scaled_int(&cursor, &remaining, voltage_context.highest_cell_voltage, 100U) != 0) return -1;
	if (append_json(&cursor, &remaining, ",\"num_valid_cells\":%d,", voltage_context.num_valid_cell_voltages) != 0) return -1;
	if (append_scaled_matrix(&cursor, &remaining, "cells_x100", voltage_context.voltage_conversions) != 0) return -1;
	if (append_json(&cursor, &remaining, "},\"temperature\":{") != 0) return -1;
	if (append_json(&cursor, &remaining, "\"average_cell_temperature_x100\":") != 0) return -1;
	if (append_json_scaled_int(&cursor, &remaining, temp_context.avg_cell_temp, 100U) != 0) return -1;
	if (append_json(&cursor, &remaining, ",\"lowest_cell_temperature_x100\":") != 0) return -1;
	if (append_json_scaled_int(&cursor, &remaining, temp_context.lowest_cell_temp, 100U) != 0) return -1;
	if (append_json(&cursor, &remaining, ",\"highest_cell_temperature_x100\":") != 0) return -1;
	if (append_json_scaled_int(&cursor, &remaining, temp_context.highest_cell_temp, 100U) != 0) return -1;
	if (append_json(&cursor, &remaining, ",\"num_valid_cells\":%d,", temp_context.num_valid_cell_temps) != 0) return -1;
	if (append_scaled_matrix(&cursor, &remaining, "cells_x100", temp_context.temp_conversions) != 0) return -1;
	if (append_json(&cursor, &remaining, "},\"current\":{") != 0) return -1;
	if (append_json(&cursor, &remaining, "\"pack_current_x100\":") != 0) return -1;
	if (append_json_scaled_int(&cursor, &remaining, current_context.current_sensor_val, 100U) != 0) return -1;
	if (append_json(&cursor, &remaining, ",\"low_sensor_current_x100\":") != 0) return -1;
	if (append_json_scaled_int(&cursor, &remaining, current_context.current_sensor_low, 100U) != 0) return -1;
	if (append_json(&cursor, &remaining, ",\"high_sensor_current_x100\":") != 0) return -1;
	if (append_json_scaled_int(&cursor, &remaining, current_context.current_sensor_high, 100U) != 0) return -1;
	if (append_json(&cursor, &remaining, "},\"soc_pct_x100\":") != 0) return -1;
	if (append_json_scaled_int(&cursor, &remaining, soc, 100U) != 0) return -1;

	if (append_json(&cursor, &remaining, ",\"specialized\":{") != 0) return -1;
	switch (bms_state) {
		case BMS_PRECHARGING:
		case BMS_DRIVE:
			if (append_json(&cursor, &remaining, "\"precharge_state\":\"%s\",\"inverter_dc_v_x100\":",
					precharge_state_to_string(precharge_state)) != 0) return -1;
			if (append_json_scaled_int(&cursor, &remaining, inverter_dc_volts, 100U) != 0) return -1;
			if (bms_state == BMS_DRIVE) {
				if (append_json(&cursor, &remaining, ",\"dcl_a_x100\":") != 0) return -1;
				if (append_json_scaled_int(&cursor, &remaining, dcl, 100U) != 0) return -1;
				if (append_json(&cursor, &remaining, ",\"ccl_a_x100\":") != 0) return -1;
				if (append_json_scaled_int(&cursor, &remaining, ccl, 100U) != 0) return -1;
			}
			break;
		case BMS_CHARGING:
			if (append_json(&cursor, &remaining,
					"\"charging_state\":\"%s\",\"control_pilot\":\"%s\",\"proximity_pilot\":\"%s\","
					"\"cp_period_us\":%lu,\"cp_pulse_us\":%lu,\"pp_adc\":%u,\"charger_fault\":%s,"
					"\"charger_starting_state_off_only\":%s,\"charger_status_raw\":%u,"
					"\"charger_status\":{\"hardware_fail\":%s,\"over_temp_protection\":%s,"
					"\"input_voltage_fault\":%s,\"starting_state_off\":%s,\"communication_timeout\":%s},"
					"\"charger_output_v_x100\":%lu,\"charger_output_a_x100\":%lu,"
					"\"advertised_amps_ac_x100\":",
					charging_state_to_string(charging_state),
					control_pilot_state_to_string(control_pilot_state),
					proximity_pilot_state_to_string(proximity_pilot_state),
					(unsigned long)j1772_context.control_pilot_period,
					(unsigned long)j1772_context.control_pilot_pulse,
					j1772_context.proximity_pilot_adc,
					chargerFaultDetected() ? "true" : "false",
					startingStateOffOnly() ? "true" : "false",
					elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.raw,
					elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.bits.hardware_fail ? "true" : "false",
					elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.bits.over_temp_protection ? "true" : "false",
					elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.bits.input_voltage_fault ? "true" : "false",
					elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.bits.starting_state_off ? "true" : "false",
					elcon_charger_context.chgmsg_18FF50E5_DF.data.status_flags.bits.communication_timeout ? "true" : "false",
					(unsigned long)elcon_charger_context.chgmsg_18FF50E5_DF.data.charger_output_voltage * 10UL,
					(unsigned long)elcon_charger_context.chgmsg_18FF50E5_DF.data.charger_output_current * 10UL) != 0) return -1;
			if (append_json_scaled_int(&cursor, &remaining, advertised_amps_ac, 100U) != 0) return -1;
			if (append_json(&cursor, &remaining, ",\"advertised_amps_dc_x100\":") != 0) return -1;
			if (append_json_scaled_int(&cursor, &remaining, advertised_amps_dc, 100U) != 0) return -1;
			break;
		case BMS_BALANCING:
			if (append_json(&cursor, &remaining,
					"\"balance_state\":\"%s\",\"balance_percent\":%u,"
					"\"balance_pwm\":%u,\"num_unbalanced_cells\":%d",
					balance_state_to_string(balance_state),
					balance_percent,
					balance_pwm,
					num_unbalanced_cells) != 0) return -1;
			break;
		default:
			if (append_json(&cursor, &remaining, "\"state_detail\":\"none\"") != 0) return -1;
			break;
	}

	if (append_json(&cursor, &remaining, "}}\r\n") != 0) return -1;
	return (int)(cursor - json_buf);
}

void send_bms_json(int json_len) {
	if (json_len <= 0) {
		return;
	}

	HAL_UART_Transmit(&hlpuart1, (uint8_t *)json_buf, (uint16_t)json_len, HAL_MAX_DELAY);
}

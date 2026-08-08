/*
 * balancing.c
 *
 *  Created on: Feb 28, 2026
 *      Author: ishanchitale
 */

#include "balancing.h"

static float voltage_conversions_snapshot[TOTAL_IC][CELLS_PER_IC];
static float local_lowest_cell_voltage;
static uint32_t phase_start_time;
volatile BalanceState balance_state = BALANCE_IDLE;
volatile int num_unbalanced_cells;
volatile uint8_t balance_percent;
volatile uint8_t balance_pwm;

void set_cell_pwm(cell_asic* ic, uint8_t ic_num, uint8_t cell_num) {
	ic[ic_num].tx_cfgb.dcc |= (1 << cell_num);

	if (cell_num < PWMA) {
		ic[ic_num].PwmA.pwma[cell_num] = balance_pwm;
	} else {
		ic[ic_num].PwmB.pwmb[cell_num - 12] = balance_pwm;
	}
}

void startBalancingLoop(int percent) {
	if (bms_state != BMS_BALANCING) return;

	if (percent <= 0) balance_percent = 0;
	else if (percent > 100) balance_percent = 100;
	else balance_percent = percent;

	balance_pwm = (uint8_t)((balance_percent*15)/100);

	if (balance_percent > 0 && balance_pwm == 0) {
		// Prevent non-zero balancing if user requested low percentage.
		// i.e. % ranging from 1-15.
		balance_pwm = 1;
	}

	balance_state = BALANCE_COMPUTE_DISCHARGE;
}

void balancingLoop(uint8_t tIC, cell_asic *ic) {
	switch (balance_state) {
		case BALANCE_IDLE:
			// DO NOTHING, BLOCK
			break;

		case BALANCE_COMPUTE_DISCHARGE:
			// CRITICAL REGION
			osMutexWait(VOLTAGE_MUTEXHandle, osWaitForever);
			memcpy(voltage_conversions_snapshot, voltage_context.voltage_conversions, sizeof(voltage_conversions_snapshot));
			local_lowest_cell_voltage = voltage_context.lowest_cell_voltage;
			osMutexRelease(VOLTAGE_MUTEXHandle);

            num_unbalanced_cells = 0;
            for (size_t i = 0; i < tIC; ++i) {
                memset(ic[i].PwmA.pwma, 0x00, sizeof(ic[i].PwmA.pwma));
                memset(ic[i].PwmB.pwmb, 0x00, sizeof(ic[i].PwmB.pwmb));
                ic[i].tx_cfgb.dcc = 0;
            }

            for (size_t i = 0; i < tIC; ++i) {
                for (size_t j = 0; j < CELLS_PER_IC; ++j) {
                    if (voltage_conversions_snapshot[i][j] > local_lowest_cell_voltage + BALANCE_VOLTAGE_THRESHOLD) {
                    	set_cell_pwm(ic, i, j);
                        num_unbalanced_cells++;
                    }
                }
            }

            if (num_unbalanced_cells > 0) {
                balance_state = BALANCE_DISCHARGE;
                phase_start_time = HAL_GetTick();
            } else {
            	balance_state = BALANCE_COMPLETE;
            }
            break;

		case BALANCE_DISCHARGE:
			// CRITICAL REGION
			osMutexWait(SPI_MUTEXHandle, osWaitForever);
            adBms6830_write_config(tIC, ic);
            adBmsWriteData(tIC, ic, WRPWM1, Pwm, A);
            adBmsWriteData(tIC, ic, WRPWM2, Pwm, B);
            osMutexRelease(SPI_MUTEXHandle);

            if (HAL_GetTick() - phase_start_time >= BALANCE_BLEED_PERIOD) {
            	for (size_t i = 0; i < tIC; ++i) {
            		memset(ic[i].PwmA.pwma, 0x00, sizeof(ic[i].PwmA.pwma));
            		memset(ic[i].PwmB.pwmb, 0x00, sizeof(ic[i].PwmB.pwmb));
            		ic[i].tx_cfgb.dcc = 0;
            	}
                balance_state = BALANCE_WAIT;
                phase_start_time = HAL_GetTick();
            }
            break;

		case BALANCE_WAIT:
			// CRITICAL REGION
			osMutexWait(SPI_MUTEXHandle, osWaitForever);
            adBms6830_write_config(tIC, ic);
            adBmsWriteData(tIC, ic, WRPWM1, Pwm, A);
            adBmsWriteData(tIC, ic, WRPWM2, Pwm, B);
            osMutexRelease(SPI_MUTEXHandle);

            if (HAL_GetTick() - phase_start_time >= BALANCE_WAIT_PERIOD) {
                balance_state = BALANCE_COMPUTE_DISCHARGE;
            }
            break;

		case BALANCE_COMPLETE:
			// DO NOTHING, BLOCK
			break;

	}
}

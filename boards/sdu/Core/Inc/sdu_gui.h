#ifndef SDU_GUI_H
#define SDU_GUI_H

#include "main.h"
#include "sdu_state.h"

/* --- Initialization --- */
/**
 * @brief Initialize the dashboard layout and screens.
 */
void UI_InitDashboard(void);

/* --- Main UI Refresh Pipeline --- */
/**
 * @brief Evaluates periodic timings and renders all dashboard text widgets,
 *        negative overbars, live profiles, and triggers screen flushes.
 * @param state Pointer to the master SDU state.
 */
void UI_UpdateRenderCycles(SDU_State_t *state);

/* --- ISR Callback Forwarding Vectors --- */
/**
 * @brief Handles OLED display DMA memory transmit complete interrupt callback.
 * @param hi2c I2C handle.
 */
void UI_HandleI2C_MasterTxCplt(I2C_HandleTypeDef *hi2c);

#endif /* SDU_GUI_H */

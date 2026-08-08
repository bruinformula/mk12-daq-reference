#ifndef TSHMU_GUI_H
#define TSHMU_GUI_H

#include "main.h"

/* --- Initialization --- */
/**
 * @brief Initialize the dashboard layout and screens.
 */
void UI_InitDashboard(void);

/* --- Main UI Refresh Pipeline --- */
/**
 * @brief Render flow rates and triggers screen flushes.
 */
void UI_UpdateRenderCycles(float flow1, uint16_t raw1, float flow2, uint16_t raw2, uint32_t now_ms);

/* --- ISR Callback Forwarding Vectors --- */
/**
 * @brief Handles OLED display DMA memory transmit complete interrupt callback.
 * @param hi2c I2C handle.
 */
void UI_HandleI2C_MasterTxCplt(I2C_HandleTypeDef *hi2c);

#endif /* TSHMU_GUI_H */

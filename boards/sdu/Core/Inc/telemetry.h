#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "main.h"
#include "sdu_state.h"

/* --- Initialization --- */
/**
 * @brief Sequentially initializes all external sensors (OLED screen, single-point IR brake temp sensor,
 *        external MCP3464 delta-sigma ADC, and MLX90640 thermal matrix camera).
 * @return HAL_StatusTypeDef HAL_OK on success, or an error status.
 */
HAL_StatusTypeDef Telemetry_InitAllSensors(void);

/* --- Main Processing Pipeline --- */
/**
 * @brief Periodically read sensors, filter raw voltage readouts, calculate suspension displacement,
 *        smooth wheel capture timings, and evaluate matrix thermal frames.
 * @param state Pointer to the master SDU state structure.
 */
void Telemetry_ProcessSensors(SDU_State_t *state);

/* --- ISR Callback Forwarding Vectors --- */
/**
 * @brief Handles the GPIO EXTI interrupt triggered by the ADC DRDY pin.
 * @param pin The GPIO pin number that triggered the interrupt.
 */
void Telemetry_HandleGPIO_EXTI(uint16_t pin);

/**
 * @brief Handles the SPI DMA Transaction Complete callback for ADC readings.
 * @param hspi SPI handle.
 */
void Telemetry_HandleSPI_TxRxCplt(SPI_HandleTypeDef *hspi);

/**
 * @brief Handles SPI Bus communication error interrupts.
 * @param hspi SPI handle.
 */
void Telemetry_HandleSPI_Error(SPI_HandleTypeDef *hspi);

/**
 * @brief Handles the Timer Input Capture callback for wheel speed sensor pulses.
 * @param htim TIM capture handle.
 */
void Telemetry_HandleTIM_IC_Capture(TIM_HandleTypeDef *htim);

/**
 * @brief Handles the non-blocking I2C memory DMA read callback for the thermal matrix camera.
 * @param hi2c I2C handle.
 */
void Telemetry_HandleI2C_MemRxCplt(I2C_HandleTypeDef *hi2c);

#endif /* TELEMETRY_H */

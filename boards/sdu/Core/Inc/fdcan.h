/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    fdcan.h
  * @brief   This file contains all the function prototypes for
  *          the fdcan.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FDCAN_H__
#define __FDCAN_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern FDCAN_HandleTypeDef hfdcan1;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_FDCAN1_Init(void);

/* USER CODE BEGIN Prototypes */

/* Unified high-fidelity 64-byte FDCAN telemetry frame structure */
typedef struct __attribute__((packed)) {
    uint16_t time_since_last_ms;     // Bytes 0-1
    struct __attribute__((packed)) {
        int16_t value;               // Value (int * scalar)
        uint16_t us_since_last_read; // us since last read
    } readings[15];                  // Bytes 2-61 (15 elements * 4 bytes = 60 bytes)
    uint8_t error_flags;             // Byte 62
    uint8_t seq_num;                 // Byte 63
} Fdcan64ByteFrame_t;

typedef struct __attribute__((packed)) {
    int16_t pixels[32];              // 32 pixels * 2 bytes = 64 bytes
} FdcanThermalFrame_t;

/* Single reusable serialization function */
HAL_StatusTypeDef CAN_Send64ByteFrame(FDCAN_TxHeaderTypeDef *txHeader, Fdcan64ByteFrame_t *data);
HAL_StatusTypeDef CAN_SendThermalFrame(FDCAN_TxHeaderTypeDef *txHeader, FdcanThermalFrame_t *data);
HAL_StatusTypeDef CAN_SendPacked64BytePayload(FDCAN_TxHeaderTypeDef *txHeader, void *payload);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __FDCAN_H__ */


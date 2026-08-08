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
#include "../../Runtime/Inc/prchg.h"
#include "charging.h"
#include "elcon_charger.h"
#include "cmsis_os.h"
/* USER CODE END Includes */

extern FDCAN_HandleTypeDef hfdcan1;

/* USER CODE BEGIN Private defines */
#define EVT_ELCON_FAULT    (1 << 2)
/* USER CODE END Private defines */

void MX_FDCAN1_Init(void);

/* USER CODE BEGIN Prototypes */
void configureFDCAN_TxMessage_STD(FDCAN_TxHeaderTypeDef* tx_msg, uint32_t std_id);
void configureFDCAN_TxMessage_EXTD(FDCAN_TxHeaderTypeDef* tx_msg, uint32_t extd_id);

extern FDCAN_FilterTypeDef sStdFilter;
extern FDCAN_FilterTypeDef sExtFilter;
void configureFilters();
void startCAN_Tx_Rx();

extern FDCAN_RxHeaderTypeDef BMS_RxHeader;
extern uint8_t BMS_RxData[8];
extern uint32_t fdcan_rx_count;
void BMS_CAN_RxHandler();
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __FDCAN_H__ */


/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#define COUNTOF(__BUFFER__)   (sizeof(__BUFFER__) / sizeof(*(__BUFFER__)))
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BALANCING_EN_Pin GPIO_PIN_13
#define BALANCING_EN_GPIO_Port GPIOC
#define RCC_OSC_OUT_Pin GPIO_PIN_1
#define RCC_OSC_OUT_GPIO_Port GPIOF
#define NEG_AIR_GND_Pin GPIO_PIN_0
#define NEG_AIR_GND_GPIO_Port GPIOC
#define POS_AIR_GND_Pin GPIO_PIN_1
#define POS_AIR_GND_GPIO_Port GPIOC
#define PRECHARGE_Pin GPIO_PIN_3
#define PRECHARGE_GPIO_Port GPIOC
#define J1772_CP_Pin GPIO_PIN_0
#define J1772_CP_GPIO_Port GPIOA
#define J1772_PP_Pin GPIO_PIN_1
#define J1772_PP_GPIO_Port GPIOA
#define LPUART1_TX_Pin GPIO_PIN_2
#define LPUART1_TX_GPIO_Port GPIOA
#define LPUART1_RX_Pin GPIO_PIN_3
#define LPUART1_RX_GPIO_Port GPIOA
#define SPI3_CS_Pin GPIO_PIN_4
#define SPI3_CS_GPIO_Port GPIOA
#define BMS_FAULT_Pin GPIO_PIN_6
#define BMS_FAULT_GPIO_Port GPIOA
#define J1772_PILOT_SWITCH_Pin GPIO_PIN_0
#define J1772_PILOT_SWITCH_GPIO_Port GPIOB
#define CURRENT_SENSOR_LOW_Pin GPIO_PIN_1
#define CURRENT_SENSOR_LOW_GPIO_Port GPIOB
#define CURRENT_SENSOR_HIGH_Pin GPIO_PIN_2
#define CURRENT_SENSOR_HIGH_GPIO_Port GPIOB
#define SHUTDOWN_POWER_Pin GPIO_PIN_10
#define SHUTDOWN_POWER_GPIO_Port GPIOB
#define SHUTDOWN_POWER_EXTI_IRQn EXTI15_10_IRQn
#define READY_SIGNAL_Pin GPIO_PIN_11
#define READY_SIGNAL_GPIO_Port GPIOB
#define SPI2_CS_Pin GPIO_PIN_12
#define SPI2_CS_GPIO_Port GPIOB
#define CHARGE_SIGNAL_Pin GPIO_PIN_9
#define CHARGE_SIGNAL_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

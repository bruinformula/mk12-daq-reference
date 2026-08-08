/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l5xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sh1106.h"
#include "mlx90614.h"
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
#include "sensor_calib_constants.h"
#include "sdu_state.h"
#include "telemetry.h"
#include "can_messaging.h"
#include "sdu_gui.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LEDARR_CLK_Pin GPIO_PIN_0
#define LEDARR_CLK_GPIO_Port GPIOA
#define LEDARR_RST_Pin GPIO_PIN_1
#define LEDARR_RST_GPIO_Port GPIOA
#define VCP_TX_Pin GPIO_PIN_2
#define VCP_TX_GPIO_Port GPIOA
#define VCP_RX_Pin GPIO_PIN_3
#define VCP_RX_GPIO_Port GPIOA
#define ADC_NSS_Pin GPIO_PIN_4
#define ADC_NSS_GPIO_Port GPIOA
#define ADC_SCK_Pin GPIO_PIN_5
#define ADC_SCK_GPIO_Port GPIOA
#define ADC_MISO_Pin GPIO_PIN_6
#define ADC_MISO_GPIO_Port GPIOA
#define ADC_MOSI_Pin GPIO_PIN_7
#define ADC_MOSI_GPIO_Port GPIOA
#define BTEMP_SCL_Pin GPIO_PIN_10
#define BTEMP_SCL_GPIO_Port GPIOB
#define BTEMP_SDA_Pin GPIO_PIN_11
#define BTEMP_SDA_GPIO_Port GPIOB
#define DISP_MEM_SCL_Pin GPIO_PIN_13
#define DISP_MEM_SCL_GPIO_Port GPIOB
#define DISP_MEM_SDA_Pin GPIO_PIN_14
#define DISP_MEM_SDA_GPIO_Port GPIOB
#define EEPROM_WP_Pin GPIO_PIN_15
#define EEPROM_WP_GPIO_Port GPIOB
#define ADC_DRDY_IRQ_Pin GPIO_PIN_8
#define ADC_DRDY_IRQ_GPIO_Port GPIOA
#define ADC_DRDY_IRQ_EXTI_IRQn EXTI8_IRQn
#define LEDARR_SER_Pin GPIO_PIN_10
#define LEDARR_SER_GPIO_Port GPIOA
#define COMP_CALIB_TOG_Pin GPIO_PIN_5
#define COMP_CALIB_TOG_GPIO_Port GPIOB
#define TTEMP_SCL_Pin GPIO_PIN_6
#define TTEMP_SCL_GPIO_Port GPIOB
#define TTEMP_SDA_Pin GPIO_PIN_7
#define TTEMP_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

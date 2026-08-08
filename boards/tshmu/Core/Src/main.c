/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "fdcan.h"
#include "i2c.h"
#include "icache.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tshmu_gui.h"
#include "sh1106.h"
#include "mcp3464.h"
#include <stdio.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct __attribute__((packed)) {
    uint16_t raw1;              // 2 bytes (raw pulses)
    uint16_t flow1;             // 2 bytes (scaled * 10.0f)
    uint16_t raw2;              // 2 bytes (raw pulses)
    uint16_t flow2;             // 2 bytes (scaled * 10.0f)
    int8_t   jitter_us;         // 1 byte (us deviation)
} DualFlowSample_t;

typedef struct __attribute__((packed)) {
    uint32_t base_timestamp;    // Bytes 0-3
    uint16_t error_flags;       // Bytes 4-5
    DualFlowSample_t samples[6]; // Bytes 6-59 (54 bytes)
    uint8_t  padding[4];        // Bytes 60-63
} DualFlowFDFrame_t;

typedef struct __attribute__((packed)) {
    int16_t temp1;              // 2 bytes
    int16_t temp2;              // 2 bytes
    int16_t temp3;              // 2 bytes
    int16_t temp4;              // 2 bytes
    int16_t temp5;              // 2 bytes
    int16_t temp6;              // 2 bytes
    int8_t  jitter_ms;          // 1 byte
} TshmuTempHistoryBlock_t;      // 13 bytes total

typedef struct __attribute__((packed)) {
    uint32_t base_timestamp;            // Bytes 0-3
    uint16_t error_flags;               // Bytes 4-5
    TshmuTempHistoryBlock_t blocks[4];  // 4 elements * 13 bytes = 52 bytes (Bytes 6-57)
    uint8_t  padding[6];                // Bytes 58-63
} TshmuTempFDFrame_t;                   // 64 bytes total
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TSHMU_BOARD_0 0
#define TSHMU_BOARD_1 1
#define ACTIVE_BOARD_ID   TSHMU_BOARD_1  // Compile-time board selection: TSHMU_BOARD_0 or TSHMU_BOARD_1

#if (ACTIVE_BOARD_ID == TSHMU_BOARD_0)
#define F_FACTOR 0.174450f
#define FLOW_OFFSET 0.809173f
#elif (ACTIVE_BOARD_ID == TSHMU_BOARD_1)
#define F_FACTOR 0.1765f
#define FLOW_OFFSET 1.58f
#else
#error "Invalid ACTIVE_BOARD_ID configured for flow calibration!"
#endif

#define THERM_T0        	298.15f    	// 25C in Kelvin
#define THERM_VREF      	3.3f
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint32_t last_report_time = 0;
uint8_t TSHMU_BOARD_ID = ACTIVE_BOARD_ID;

MCP3464_Handle tshmuAdc = { ALG_SPI_CS_GPIO_Port, ALG_SPI_CS_Pin, &hspi1 };

#if (ACTIVE_BOARD_ID == TSHMU_BOARD_0)
// Board 0 calibrated/nominal values
static const float therm_R_series[6] = {10000.0f, 10000.0f, 10000.0f, 10000.0f, 10000.0f, 10000.0f};
static const float therm_R0[6] = {50000.0f, 10000.0f, 10000.0f, 10000.0f, 10000.0f, 10000.0f};
static const float therm_Beta[6] = {3950.0f, 3694.0f, 3694.0f, 3694.0f, 3694.0f, 3694.0f};
static const float therm_Offset[6] = {0.0f, -1.7f, -1.7f, -1.7f, -1.7f, -1.7f};
#elif (ACTIVE_BOARD_ID == TSHMU_BOARD_1)
// Board 1 calibrated/nominal values
static const float therm_R_series[6] = {10000.0f, 10000.0f, 10000.0f, 10000.0f, 10000.0f, 10000.0f};
static const float therm_R0[6] = {10000.0f, 10000.0f, 10000.0f, 10000.0f, 10000.0f, 10000.0f};
static const float therm_Beta[6] = {3694.0f, 3694.0f, 3694.0f, 3694.0f, 3694.0f, 3694.0f};
static const float therm_Offset[6] = {-1.7f, -1.7f, -1.7f, -1.7f, -1.7f, -1.7f};
#else
#error "Invalid ACTIVE_BOARD_ID configured!"
#endif

// Thermistor channels buffer & results
volatile float therm_voltages[6] = {0};
volatile float therm_temperatures[6] = {0};
uint32_t sampleCount = 0; // needed by mcp3464.c

// CAN Variables
FDCAN_TxHeaderTypeDef canFlowTxHeader;
DualFlowFDFrame_t flow_tx_frame = {0};
uint8_t flow_sample_idx = 0;
uint32_t flow_base_timestamp_ms = 0;

FDCAN_TxHeaderTypeDef canTempTxHeader;
TshmuTempFDFrame_t temp_tx_frame = {0};
uint8_t temp_sample_idx = 0;
uint32_t temp_base_timestamp_ms = 0;
uint32_t last_temp_time = 0;

volatile uint8_t can_tec = 0;
volatile uint8_t can_rec = 0;
volatile uint32_t can_psr = 0;
volatile HAL_StatusTypeDef can_send_status = HAL_OK;
volatile uint32_t can_tx_count = 0;
volatile uint32_t fdcan_rx_count = 0;
volatile uint32_t can_error_count = 0;
volatile uint32_t can_last_error = 0;

// Variables for FLOW1 (TIM1_CH2)
volatile uint32_t last_capture_1 = 0;
volatile uint32_t current_capture_1 = 0;
volatile uint32_t diff_capture_1 = 0;
volatile uint8_t first_capture_1 = 0;
volatile float input_frequency_1 = 0.0f;
volatile uint32_t pulse_count_1 = 0;
volatile uint32_t cumulative_pulses_1 = 0;
volatile uint32_t last_capture_time_1 = 0;
volatile float inst_flowRate_1 = 0.0f;
volatile float acc_flowRate_1 = 0.0f;

// Variables for FLOW2 (TIM3_CH2)
volatile uint32_t last_capture_2 = 0;
volatile uint32_t current_capture_2 = 0;
volatile uint32_t diff_capture_2 = 0;
volatile uint8_t first_capture_2 = 0;
volatile float input_frequency_2 = 0.0f;
volatile uint32_t pulse_count_2 = 0;
volatile uint32_t cumulative_pulses_2 = 0;
volatile uint32_t last_capture_time_2 = 0;
volatile float inst_flowRate_2 = 0.0f;
volatile float acc_flowRate_2 = 0.0f;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
float Thermistor_Voltage_To_Resistance(float voltage, float rSeries);
float Thermistor_Resistance_To_Kelvin(float resistance, float r0, float beta);
float Thermistor_Voltage_To_Celsius(float voltage, float rSeries, float r0, float beta);
float Get_Temperature_From_Table(float voltage, float rSeries);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _write(int file, char *ptr, int len) {
  HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, HAL_MAX_DELAY);
  return len;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_FDCAN1_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_ICACHE_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  /* Manually configure PB2 as output low to enable the CAN transceiver (Standby pin) */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
  GPIO_InitTypeDef GPIO_InitStructPB2 = {0};
  GPIO_InitStructPB2.Pin = GPIO_PIN_2;
  GPIO_InitStructPB2.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStructPB2.Pull = GPIO_PULLDOWN;
  GPIO_InitStructPB2.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStructPB2);

  SH1106_Init();
  UI_InitDashboard();

  HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_2);
  HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_2);
  
  canFlowTxHeader.Identifier = 0x100 | (TSHMU_BOARD_ID << 3) | 0x02; // Dynamically sets 0x102 (TSHMU 0) or 0x10A (TSHMU 1)
  canFlowTxHeader.IdType = FDCAN_STANDARD_ID;
  canFlowTxHeader.TxFrameType = FDCAN_DATA_FRAME;
  canFlowTxHeader.DataLength = FDCAN_DLC_BYTES_64;
  canFlowTxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  canFlowTxHeader.BitRateSwitch = FDCAN_BRS_OFF;
  canFlowTxHeader.FDFormat = FDCAN_FD_CAN;
  canFlowTxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  canFlowTxHeader.MessageMarker = 0;

  canTempTxHeader.Identifier = 0x100 | (TSHMU_BOARD_ID << 3) | 0x03; // Dynamically sets 0x103 (TSHMU 0) or 0x10B (TSHMU 1)
  canTempTxHeader.IdType = FDCAN_STANDARD_ID;
  canTempTxHeader.TxFrameType = FDCAN_DATA_FRAME;
  canTempTxHeader.DataLength = FDCAN_DLC_BYTES_64;
  canTempTxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  canTempTxHeader.BitRateSwitch = FDCAN_BRS_OFF;
  canTempTxHeader.FDFormat = FDCAN_FD_CAN;
  canTempTxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  canTempTxHeader.MessageMarker = 0;

  FDCAN_FilterTypeDef sFilterConfig = {0};
  sFilterConfig.IdType = FDCAN_STANDARD_ID;
  sFilterConfig.FilterIndex = 0;
  sFilterConfig.FilterType = FDCAN_FILTER_RANGE;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1 = 0x000;
  sFilterConfig.FilterID2 = 0x7FF;

  if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_FDCAN_Start(&hfdcan1);

  /* Deassert SPI CS pin — CubeMX initializes CS low */
  HAL_GPIO_WritePin(ALG_SPI_CS_GPIO_Port, ALG_SPI_CS_Pin, GPIO_PIN_SET);

  /* Initialize MCP3464 external ADC in SCAN mode */
  MCP3464_Init(&tshmuAdc);
  MCP3464_SetMode(&tshmuAdc, MCP3464_MODE_SCAN, 0, OSR_2048);

  /* Clear stale EXTI flags and start continuous DMA read */
  __HAL_GPIO_EXTI_CLEAR_FLAG(ALG_SPI_nIRQ_Pin);
  MCP3464_StartContinuous_DMA(&tshmuAdc);

  /* Trigger the first read if the IRQ pin is already low */
  if (HAL_GPIO_ReadPin(ALG_SPI_nIRQ_GPIO_Port, ALG_SPI_nIRQ_Pin) == GPIO_PIN_RESET) {
    MCP3464_ArmRead(&tshmuAdc);
  }

  last_report_time = HAL_GetTick();
  last_temp_time = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now = HAL_GetTick();
    if ((now - last_report_time) >= 100)
    {
      uint32_t elapsed_ms = now - last_report_time;
      last_report_time = now;
      
      // FLOW1 Calculation
      float inst_freq_1 = 0.0f;
      if ((now - last_capture_time_1) < 1500 && first_capture_1)
      {
        __disable_irq();
        inst_freq_1 = input_frequency_1;
        __enable_irq();
      }
      
      if (inst_freq_1 > 0.0f) {
        inst_flowRate_1 = (inst_freq_1 * F_FACTOR) + FLOW_OFFSET;
      } else {
        inst_flowRate_1 = 0.0f;
      }
      
      __disable_irq();
      uint32_t current_pulses_1 = pulse_count_1;
      pulse_count_1 = 0;
      __enable_irq();
      
      float acc_freq_1 = ((float)current_pulses_1 * 1000.0f) / (float)elapsed_ms;
      if (current_pulses_1 > 0) {
        acc_flowRate_1 = (acc_freq_1 * F_FACTOR) + FLOW_OFFSET;
      } else {
        acc_flowRate_1 = 0.0f;
      }

      // FLOW2 Calculation
      float inst_freq_2 = 0.0f;
      if ((now - last_capture_time_2) < 1500 && first_capture_2)
      {
        __disable_irq();
        inst_freq_2 = input_frequency_2;
        __enable_irq();
      }
      
      if (inst_freq_2 > 0.0f) {
        inst_flowRate_2 = (inst_freq_2 * F_FACTOR) + FLOW_OFFSET;
      } else {
        inst_flowRate_2 = 0.0f;
      }
      
      __disable_irq();
      uint32_t current_pulses_2 = pulse_count_2;
      pulse_count_2 = 0;
      __enable_irq();
      
      float acc_freq_2 = ((float)current_pulses_2 * 1000.0f) / (float)elapsed_ms;
      if (current_pulses_2 > 0) {
        acc_flowRate_2 = (acc_freq_2 * F_FACTOR) + FLOW_OFFSET;
      } else {
        acc_flowRate_2 = 0.0f;
      }
      
      printf("%lu,%lu,%d,%lu,%d\r\n", now, current_pulses_1, (int)(inst_flowRate_1 * 1000), current_pulses_2, (int)(inst_flowRate_2 * 1000));
      
      // CAN Data Packing
      if (flow_sample_idx == 0)
      {
        flow_base_timestamp_ms = now;
      }
      
      int32_t deviation_ms = (int32_t)elapsed_ms - 100;
      int32_t jitter_us = deviation_ms * 1000;
      if (jitter_us > 127) jitter_us = 127;
      if (jitter_us < -128) jitter_us = -128;
      
      UI_UpdateRenderCycles(inst_flowRate_1, current_pulses_1, inst_flowRate_2, current_pulses_2, now);

      if (tshmuAdc.running && !tshmuAdc.transferPending &&
          HAL_GPIO_ReadPin(ALG_SPI_nIRQ_GPIO_Port, ALG_SPI_nIRQ_Pin) == GPIO_PIN_RESET)
      {
        MCP3464_ArmRead(&tshmuAdc);
      }

      if (flow_sample_idx < 6)
      {
        flow_tx_frame.samples[flow_sample_idx].raw1 = (uint16_t)current_pulses_1;
        flow_tx_frame.samples[flow_sample_idx].flow1 = (uint16_t)(inst_flowRate_1 * 10.0f);
        flow_tx_frame.samples[flow_sample_idx].raw2 = (uint16_t)current_pulses_2;
        flow_tx_frame.samples[flow_sample_idx].flow2 = (uint16_t)(inst_flowRate_2 * 10.0f);
        flow_tx_frame.samples[flow_sample_idx].jitter_us = (int8_t)jitter_us;
        flow_sample_idx++;
      }
      
      if (flow_sample_idx >= 6)
      {
        flow_tx_frame.base_timestamp = flow_base_timestamp_ms;
        flow_tx_frame.error_flags = 0; // Simple error flags for now
        
        HAL_StatusTypeDef tx_status = HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &canFlowTxHeader, (uint8_t*)&flow_tx_frame);
        can_send_status = tx_status;
        if (tx_status == HAL_OK) {
          can_tx_count++;
        } else {
          can_error_count++;
          can_last_error = hfdcan1.ErrorCode;
        }
        
        flow_sample_idx = 0;
      }
    }

    // Temperature Sampling & Transmission
    if ((now - last_temp_time) >= 150)
    {
      uint32_t elapsed_temp_ms = now - last_temp_time;
      last_temp_time = now;

      if (temp_sample_idx == 0)
      {
        temp_base_timestamp_ms = now;
      }

      int32_t deviation_ms = (int32_t)elapsed_temp_ms - 150;
      if (deviation_ms > 127) deviation_ms = 127;
      if (deviation_ms < -128) deviation_ms = -128;

      if (temp_sample_idx < 4)
      {
        temp_tx_frame.blocks[temp_sample_idx].temp1 = (int16_t)(therm_voltages[0] * 1000.0f);
        temp_tx_frame.blocks[temp_sample_idx].temp2 = (int16_t)(therm_voltages[1] * 1000.0f);
        temp_tx_frame.blocks[temp_sample_idx].temp3 = (int16_t)(therm_voltages[2] * 1000.0f);
        temp_tx_frame.blocks[temp_sample_idx].temp4 = (int16_t)(therm_voltages[3] * 1000.0f);
        temp_tx_frame.blocks[temp_sample_idx].temp5 = (int16_t)(therm_voltages[4] * 1000.0f);
        temp_tx_frame.blocks[temp_sample_idx].temp6 = (int16_t)(therm_voltages[5] * 1000.0f);
        temp_tx_frame.blocks[temp_sample_idx].jitter_ms = (int8_t)deviation_ms;
        temp_sample_idx++;
      }

      if (temp_sample_idx >= 4)
      {
        temp_tx_frame.base_timestamp = temp_base_timestamp_ms;
        temp_tx_frame.error_flags = 0;
        memset(temp_tx_frame.padding, 0, sizeof(temp_tx_frame.padding));

        HAL_StatusTypeDef tx_status = HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &canTempTxHeader, (uint8_t*)&temp_tx_frame);
        can_send_status = tx_status;
        if (tx_status == HAL_OK) {
          can_tx_count++;
        } else {
          can_error_count++;
          can_last_error = hfdcan1.ErrorCode;
        }

        temp_sample_idx = 0;
      }
      /* Poll FDCAN status registers for live diagnostics */
      can_tec = (FDCAN1->ECR >> 16) & 0xFF;
      can_rec = FDCAN1->ECR & 0xFF;
      can_psr = FDCAN1->PSR;
    }

    // Poll received CAN messages to detect network activity
    if (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) > 0)
    {
      FDCAN_RxHeaderTypeDef rxHeader;
      uint8_t rxData[64];
      if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK)
      {
        fdcan_rx_count++;
      }
    }

    // Stuck-transmission watchdog & Bus-Off recovery (1Hz periodic execution)
    static uint32_t last_watchdog_time_ms = 0;
    static uint8_t can_network_detected = 0U;
    static uint32_t last_tx_count = 0U;
    static uint32_t tx_stuck_seconds = 0U;

    if (now - last_watchdog_time_ms >= 1000)
    {
      last_watchdog_time_ms = now;

      if (!can_network_detected && fdcan_rx_count > 0U)
      {
        can_network_detected = 1U;
      }

      if (can_network_detected)
      {
        if (hfdcan1.Instance != NULL && hfdcan1.Instance->TXBRP != 0U && can_tx_count == last_tx_count)
        {
          tx_stuck_seconds++;
          if (tx_stuck_seconds >= 5U)
          {
            // Reset system to recover FDCAN state
            NVIC_SystemReset();
          }
        }
        else
        {
          tx_stuck_seconds = 0U;
          last_tx_count = can_tx_count;
        }
      }
      else
      {
        last_tx_count = can_tx_count;
      }

      /* Auto-recover from bus-off */
      FDCAN_ProtocolStatusTypeDef psr = {0};
      if (HAL_FDCAN_GetProtocolStatus(&hfdcan1, &psr) == HAL_OK)
      {
        if (psr.BusOff != 0U)
        {
          (void)HAL_FDCAN_Start(&hfdcan1);
        }
      }
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 55;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
  UI_HandleI2C_MasterTxCplt(hi2c);
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
  {
    current_capture_1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
    pulse_count_1++;
    cumulative_pulses_1++;
    last_capture_time_1 = HAL_GetTick();
    
    if (first_capture_1 == 0)
    {
      last_capture_1 = current_capture_1;
      first_capture_1 = 1;
    }
    else
    {
      uint32_t diff = 0;
      if (current_capture_1 >= last_capture_1)
      {
        diff = current_capture_1 - last_capture_1;
      }
      else
      {
        diff = (65535U - last_capture_1) + current_capture_1;
      }
      
      if (diff > 5)
      {
        diff_capture_1 = diff;
        input_frequency_1 = 50000.0f / (float)diff_capture_1;
      }
      last_capture_1 = current_capture_1;
    }
  }
  else if (htim->Instance == TIM3 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
  {
    current_capture_2 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
    pulse_count_2++;
    cumulative_pulses_2++;
    last_capture_time_2 = HAL_GetTick();
    
    if (first_capture_2 == 0)
    {
      last_capture_2 = current_capture_2;
      first_capture_2 = 1;
    }
    else
    {
      uint32_t diff = 0;
      if (current_capture_2 >= last_capture_2)
      {
        diff = current_capture_2 - last_capture_2;
      }
      else
      {
        diff = (65535U - last_capture_2) + current_capture_2;
      }
      
      if (diff > 5)
      {
        diff_capture_2 = diff;
        input_frequency_2 = 50000.0f / (float)diff_capture_2;
      }
      last_capture_2 = current_capture_2;
    }
  }
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == ALG_SPI_nIRQ_Pin)
  {
    MCP3464_ArmRead(&tshmuAdc);
  }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
  if (hspi->Instance == SPI1) {
    if (tshmuAdc.config.reg.config3.DATA_FORMAT == FORMAT_32_CHID) {
      uint8_t raw_ch = (tshmuAdc.rxBuf[1] >> 4);
      if (raw_ch < 6) {
        // Correct channel routing mapping (raw 0<->1, 2<->3, 4<->5)
        uint8_t chID = raw_ch ^ 1;
        uint16_t uraw = ((uint16_t)tshmuAdc.rxBuf[3] << 8) | tshmuAdc.rxBuf[4];
        uint8_t sgn = tshmuAdc.rxBuf[1] & 0x0F;
        float voltage;
        if (sgn == 0x0F) {
          voltage = (float)(int16_t)uraw * (THERM_VREF / 32768.0f);
        } else {
          voltage = (float)uraw * (THERM_VREF / 32768.0f);
        }
        therm_voltages[chID] = voltage;
        
        float new_temp;
        if (therm_Beta[chID] == 3694.0f) {
          new_temp = Get_Temperature_From_Table(voltage, therm_R_series[chID]) + therm_Offset[chID];
        } else {
          if (voltage <= 0.0f || voltage >= (THERM_VREF - 0.001f)) {
            new_temp = 0.0f;
          } else {
            new_temp = Thermistor_Voltage_To_Celsius(voltage, therm_R_series[chID], therm_R0[chID], therm_Beta[chID]) + therm_Offset[chID];
          }
        }
        
        // Exponential Moving Average (EMA) software filter to remove ~40mV of electrical noise
        if (therm_temperatures[chID] == 0.0f) {
            therm_temperatures[chID] = new_temp; // Initialize instantly on boot
        } else {
            therm_temperatures[chID] = (therm_temperatures[chID] * 0.9f) + (new_temp * 0.1f);
        }
      }
    }
    MCP3464_TxRxCpltCallback(&tshmuAdc);
  }
}

// --- Configuration ---
#define GA10K_TABLE_SIZE 76

/* * Binary Search Lookup Table: Resistance -> Temperature (°C)
 * Based directly on the empirical resistance table provided.
 * Range: 10C (18787 ohm) to 85C (1255 ohm) in 1C steps.
 */
static const float ga10k_res_table[GA10K_TABLE_SIZE] = {
    18787.0f, 17983.0f, 17219.0f, 16490.0f, 15797.0f, 15136.0f, 14506.0f, 13906.0f,
    13334.0f, 12788.0f, 12268.0f, 11771.0f, 11297.0f, 10845.0f, 10413.0f, 10000.0f,
    9606.0f, 9229.0f, 8869.0f, 8525.0f, 8196.0f, 7882.0f, 7581.0f, 7293.0f,
    7018.0f, 6754.0f, 6501.0f, 6260.0f, 6028.0f, 5806.0f, 5594.0f, 5390.0f,
    5195.0f, 5007.0f, 4828.0f, 4656.0f, 4490.0f, 4332.0f, 4180.0f, 4034.0f,
    3893.0f, 3759.0f, 3629.0f, 3505.0f, 3386.0f, 3271.0f, 3160.0f, 3054.0f,
    2952.0f, 2854.0f, 2760.0f, 2669.0f, 2582.0f, 2498.0f, 2417.0f, 2339.0f,
    2264.0f, 2191.0f, 2122.0f, 2055.0f, 1990.0f, 1928.0f, 1868.0f, 1810.0f,
    1754.0f, 1700.0f, 1648.0f, 1598.0f, 1550.0f, 1503.0f, 1458.0f, 1414.0f,
    1372.0f, 1332.0f, 1293.0f, 1255.0f
};

/**
 * @brief  Converts a measured voltage to Temperature via empirical resistance LUT.
 * @param  voltage  Voltage reading (0.0f to THERM_VREF)
 * @param  rSeries  Series pull-up resistor value
 * @retval float    Interpolated Temperature in Celsius
 */
float Get_Temperature_From_Table(float voltage, float rSeries) {
    if (voltage <= 0.001f) return 125.0f; // Short circuit
    if (voltage >= (THERM_VREF - 0.001f)) return -40.0f; // Open circuit

    // 1. Calculate actual resistance
    float resistance = rSeries * voltage / (THERM_VREF - voltage);

    // 2. Bounds checking
    if (resistance >= ga10k_res_table[0]) {
        return 10.0f; // Cap at bottom of table
    }
    if (resistance <= ga10k_res_table[GA10K_TABLE_SIZE - 1]) {
        return 85.0f; // Cap at top of table
    }

    // 3. Binary search for closest bounding resistances
    // Note: The array is strictly decreasing!
    int low = 0;
    int high = GA10K_TABLE_SIZE - 1;
    while (high - low > 1) {
        int mid = (low + high) >> 1;
        if (resistance < ga10k_res_table[mid]) {
            low = mid; 
        } else {
            high = mid;
        }
    }

    // 4. Linear interpolation between ga10k_res_table[low] and ga10k_res_table[high]
    float r_low = ga10k_res_table[low];
    float r_high = ga10k_res_table[high];
    
    // Fraction of the way from r_low down to r_high
    float fraction = (r_low - resistance) / (r_low - r_high);

    // Table starts at 10.0C, and each index adds 1.0C
    return 10.0f + (float)low + fraction;
}

float Thermistor_Voltage_To_Resistance(float voltage, float rSeries)
{
  if (voltage <= 0.001f)
    voltage = 0.001f;
  if (voltage >= (THERM_VREF - 0.001f))
    voltage = THERM_VREF - 0.001f;
  // Low-side thermistor configuration:
  // Vout = Vref * Rtherm / (Rpullup + Rtherm) -> Rtherm = Rpullup * Vout / (Vref - Vout)
  return rSeries * voltage / (THERM_VREF - voltage);
}

float Thermistor_Resistance_To_Kelvin(float resistance, float r0, float beta)
{
  float invT = (1.0f / THERM_T0) + (1.0f / beta) * logf(resistance / r0);
  return 1.0f / invT;
}

float Thermistor_Voltage_To_Celsius(float voltage, float rSeries, float r0, float beta)
{
  float resistance = Thermistor_Voltage_To_Resistance(voltage, rSeries);
  float tempK = Thermistor_Resistance_To_Kelvin(resistance, r0, beta);
  return tempK - 273.15f;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

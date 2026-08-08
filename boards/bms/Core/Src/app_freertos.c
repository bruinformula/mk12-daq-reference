/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "freertos_handles.h"
#include "voltage_calculations.h"
#include "thermistor.h"
#include "current_calculations.h"
#include "curr_limiting.h"
#include "prchg.h"
#include "gui.h"
#include "bms_state.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
volatile bool temp_ready;
volatile bool voltage_ready;
/* USER CODE END Variables */
osThreadId voltageTaskHandle;
osThreadId tempTaskHandle;
osThreadId safetyTaskHandle;
osThreadId currTaskHandle;
osThreadId dataloggingTaskHandle;
osThreadId prchgTaskHandle;
osThreadId currLimitTaskHandle;
osThreadId balancingTaskHandle;
osThreadId chargingTaskHandle;
osThreadId socTaskHandle;
osThreadId serialCmdTaskHandle;
osMutexId SPI_MUTEXHandle;
osMutexId CAN_MUTEXHandle;
osMutexId VOLTAGE_MUTEXHandle;
osMutexId TEMP_MUTEXHandle;
osMutexId FAULT_MUTEXHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void voltageFunction(void const * argument);
void tempFunction(void const * argument);
void safetyFunction(void const * argument);
void currFunction(void const * argument);
void dataloggingFunction(void const * argument);
void prchgFunction(void const * argument);
void currLimitFunction(void const * argument);
void balancingFunction(void const * argument);
void chargingFunction(void const * argument);
void socFunction(void const * argument);
void serialCmdFunction(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* definition and creation of SPI_MUTEX */
  osMutexDef(SPI_MUTEX);
  SPI_MUTEXHandle = osMutexCreate(osMutex(SPI_MUTEX));

  /* definition and creation of CAN_MUTEX */
  osMutexDef(CAN_MUTEX);
  CAN_MUTEXHandle = osMutexCreate(osMutex(CAN_MUTEX));

  /* definition and creation of VOLTAGE_MUTEX */
  osMutexDef(VOLTAGE_MUTEX);
  VOLTAGE_MUTEXHandle = osMutexCreate(osMutex(VOLTAGE_MUTEX));

  /* definition and creation of TEMP_MUTEX */
  osMutexDef(TEMP_MUTEX);
  TEMP_MUTEXHandle = osMutexCreate(osMutex(TEMP_MUTEX));

  /* definition and creation of FAULT_MUTEX */
  osMutexDef(FAULT_MUTEX);
  FAULT_MUTEXHandle = osMutexCreate(osMutex(FAULT_MUTEX));

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of voltageTask */
  osThreadDef(voltageTask, voltageFunction, osPriorityAboveNormal, 0, 512);
  voltageTaskHandle = osThreadCreate(osThread(voltageTask), NULL);

  /* definition and creation of tempTask */
  osThreadDef(tempTask, tempFunction, osPriorityAboveNormal, 0, 512);
  tempTaskHandle = osThreadCreate(osThread(tempTask), NULL);

  /* definition and creation of safetyTask */
  osThreadDef(safetyTask, safetyFunction, osPriorityHigh, 0, 512);
  safetyTaskHandle = osThreadCreate(osThread(safetyTask), NULL);

  /* definition and creation of currTask */
  osThreadDef(currTask, currFunction, osPriorityAboveNormal, 0, 512);
  currTaskHandle = osThreadCreate(osThread(currTask), NULL);

  /* definition and creation of dataloggingTask */
  osThreadDef(dataloggingTask, dataloggingFunction, osPriorityBelowNormal, 0, 1024);
  dataloggingTaskHandle = osThreadCreate(osThread(dataloggingTask), NULL);

  /* definition and creation of prchgTask */
  osThreadDef(prchgTask, prchgFunction, osPriorityNormal, 0, 1024);
  prchgTaskHandle = osThreadCreate(osThread(prchgTask), NULL);

  /* definition and creation of currLimitTask */
  osThreadDef(currLimitTask, currLimitFunction, osPriorityNormal, 0, 512);
  currLimitTaskHandle = osThreadCreate(osThread(currLimitTask), NULL);

  /* definition and creation of balancingTask */
  osThreadDef(balancingTask, balancingFunction, osPriorityNormal, 0, 1024);
  balancingTaskHandle = osThreadCreate(osThread(balancingTask), NULL);

  /* definition and creation of chargingTask */
  osThreadDef(chargingTask, chargingFunction, osPriorityNormal, 0, 1024);
  chargingTaskHandle = osThreadCreate(osThread(chargingTask), NULL);

  /* definition and creation of socTask */
  osThreadDef(socTask, socFunction, osPriorityNormal, 0, 512);
  socTaskHandle = osThreadCreate(osThread(socTask), NULL);

  /* definition and creation of serialCmdTask */
  osThreadDef(serialCmdTask, serialCmdFunction, osPriorityNormal, 0, 512);
  serialCmdTaskHandle = osThreadCreate(osThread(serialCmdTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_voltageFunction */
/**
  * @brief  Function implementing the voltageTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_voltageFunction */
void voltageFunction(void const * argument)
{
  /* USER CODE BEGIN voltageFunction */
  static bool first_run = false;
  /* Infinite loop */
  for(;;)
  {
	computeAllVoltages(TOTAL_IC, IC);
	if (!first_run) {
		first_run = true;
		voltage_ready = true;
	}
	osDelay(500);
  }
  /* USER CODE END voltageFunction */
}

/* USER CODE BEGIN Header_tempFunction */
/**
* @brief Function implementing the tempTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_tempFunction */
void tempFunction(void const * argument)
{
  /* USER CODE BEGIN tempFunction */
  static bool first_run = false;
  /* Infinite loop */
  for(;;)
  {
	computeAllTemps(TOTAL_IC, IC);
	if (!first_run) {
		first_run = true;
		temp_ready = true;
	}
    osDelay(1000);
  }
  /* USER CODE END tempFunction */
}

/* USER CODE BEGIN Header_safetyFunction */
/**
* @brief Function implementing the safetyTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_safetyFunction */
void safetyFunction(void const * argument)
{
  /* USER CODE BEGIN safetyFunction */

  /* Infinite loop */
  for(;;)
  {
	service_shutdown_power_signal();
	BMS_CheckFaultRegister();
    osDelay(50);
  }
  /* USER CODE END safetyFunction */
}

/* USER CODE BEGIN Header_currFunction */
/**
* @brief Function implementing the currTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_currFunction */
void currFunction(void const * argument)
{
  /* USER CODE BEGIN currFunction */
  /* Infinite loop */
  for(;;)
  {
	  ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Based on ADC ISR; ~70 Hz.
	  calculateCurrent();
  }
  /* USER CODE END currFunction */
}

/* USER CODE BEGIN Header_dataloggingFunction */
/**
* @brief Function implementing the dataloggingTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_dataloggingFunction */
void dataloggingFunction(void const * argument)
{
  /* USER CODE BEGIN dataloggingFunction */
  /* Infinite loop */
  for(;;)
  {
#if BMS_CAN_DATALOGGING_MODE == BMS_DATALOGGING_ENABLED
	sendVoltage();
	sendTemp();
	sendSoc_Curr_Pack();
#endif

#if BMS_JSON_DATALOGGING_MODE == BMS_DATALOGGING_ENABLED
	taskENTER_CRITICAL();
	int json_len = build_bms_json();
	taskEXIT_CRITICAL();
	send_bms_json(json_len);
#endif

    osDelay(1000);
  }
  /* USER CODE END dataloggingFunction */
}

/* USER CODE BEGIN Header_prchgFunction */
/**
* @brief Function implementing the prchgTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_prchgFunction */
void prchgFunction(void const * argument)
{
  /* USER CODE BEGIN prchgFunction */
  /* Infinite loop */
  for(;;)
  {
	  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
	  while (bms_state == BMS_PRECHARGING) {
		  precharge_loop();
		  osDelay(50);
	  }
  }
  /* USER CODE END prchgFunction */
}

/* USER CODE BEGIN Header_currLimitFunction */
/**
* @brief Function implementing the currLimitTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_currLimitFunction */
void currLimitFunction(void const * argument)
{
  /* USER CODE BEGIN currLimitFunction */
  /* Infinite loop */
  for(;;)
  {
	  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
	  while (bms_state == BMS_DRIVE) {
//		  sendDCL_CCL();
		  osDelay(100);
	  }
  }
  /* USER CODE END currLimitFunction */
}

/* USER CODE BEGIN Header_balancingFunction */
/**
* @brief Function implementing the balancingTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_balancingFunction */
void balancingFunction(void const * argument)
{
  /* USER CODE BEGIN balancingFunction */
  /* Infinite loop */
  for(;;)
  {
	  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
	  while (bms_state == BMS_BALANCING) {
		  balancingLoop(TOTAL_IC, IC);
		  osDelay(500); // Discharge Timer is ~2 Seconds.
	  }
  }
  /* USER CODE END balancingFunction */
}

/* USER CODE BEGIN Header_chargingFunction */
/**
* @brief Function implementing the chargingTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_chargingFunction */
void chargingFunction(void const * argument)
{
  /* USER CODE BEGIN chargingFunction */
  /* Infinite loop */
  for(;;)
  {
	  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
	  while (bms_state == BMS_CHARGING) {
		  // Waits for ISR Notifications, w/ 50 ms timeout.
		  charging_loop();
	  }
  }
  /* USER CODE END chargingFunction */
}

/* USER CODE BEGIN Header_socFunction */
/**
* @brief Function implementing the socTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_socFunction */
void socFunction(void const * argument)
{
  /* USER CODE BEGIN socFunction */
  static uint32_t last_tick;
  while (!(voltage_ready && temp_ready)) {
	  osDelay(50);
  }
  get_initial_soc();
  last_tick = HAL_GetTick();
  /* Infinite loop */
  for(;;)
  {
	  uint32_t now = HAL_GetTick();
	  uint32_t delta = now - last_tick;
	  last_tick = now;
	  coulomb_count(delta);
	  osDelay(500);
  }
  /* USER CODE END socFunction */
}

/* USER CODE BEGIN Header_serialCmdFunction */
/**
* @brief Function implementing the serialCmdTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_serialCmdFunction */
void serialCmdFunction(void const * argument)
{
  /* USER CODE BEGIN serialCmdFunction */
  /* Infinite loop */
  for(;;)
  {
	ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
	processGUI_Cmd();
  }
  /* USER CODE END serialCmdFunction */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */


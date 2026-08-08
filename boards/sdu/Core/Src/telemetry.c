#include "telemetry.h"
#include "i2c.h"
#include "spi.h"
#include <stdio.h>
#include <string.h>

/* --- External Peripheral Handles --- */
extern SPI_HandleTypeDef hspi1;
extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c4;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;

/* --- Global Diagnostic Variables (extern'd by drivers) --- */
volatile uint32_t sampleCount = 0;

/* --- Private Static Hardware Driver Handles & Buffers --- */
static MCP3464_Handle hmcp3464;

static uint16_t eeData[832];
static paramsMLX90640 mlxParams;
static uint16_t frameData[834]; // 768 px + 64 aux + 2 ctrl
static float tempImage[768];    // Calculated temperatures (°C)

/* --- Wheel Speed Sensor ISR Registers --- */
static volatile uint32_t isr_last_capture = 0;
static volatile uint32_t isr_current_capture = 0;
static volatile uint32_t isr_diff_capture = 0;
static volatile uint8_t isr_first_capture = 0;
static volatile float isr_input_freq = 0.0f;
static volatile uint32_t isr_whs_trigger_count = 0;
static volatile uint32_t isr_last_interrupt_timestamp = 0;
static volatile uint8_t isr_whs_data_ready = 0;

/* --- Matrix Thermal Camera Private State --- */
static volatile uint8_t mlx_frame_ready = 0;
bool mlx90640_detected = false;

/* --- Exponential Backoff Retry variables --- */
static uint32_t mlx_next_retry_time_ms = 0;
static uint32_t mlx_retry_interval_ms = 3000;
static uint32_t mlx_last_success_time_ms = 0;
static const uint32_t MLX_MAX_RETRY_INTERVAL_MS = 60000;
static const uint32_t MLX_TIMEOUT_MS = 3000;

static bool adc_initialized = false;
static uint32_t adc_next_retry_time_ms = 0;
static uint32_t adc_retry_interval_ms = 3000;
static uint32_t adc_last_success_time_ms = 0;
static const uint32_t ADC_MAX_RETRY_INTERVAL_MS = 60000;
static const uint32_t ADC_TIMEOUT_MS = 1000;

static void I2C1_ClearBus(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOB_CLK_ENABLE();

  // Configure SCL (PB6) as Output Open-Drain with pull-up
  GPIO_InitStruct.Pin = TTEMP_SCL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(TTEMP_SCL_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(TTEMP_SCL_GPIO_Port, TTEMP_SCL_Pin, GPIO_PIN_SET);

  // Configure SDA (PB7) as Input with pull-up
  GPIO_InitStruct.Pin = TTEMP_SDA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(TTEMP_SDA_GPIO_Port, &GPIO_InitStruct);

  HAL_Delay(1);

  // If SDA is held low by a hung sensor, pulse SCL 9 times to free it
  if (HAL_GPIO_ReadPin(TTEMP_SDA_GPIO_Port, TTEMP_SDA_Pin) == GPIO_PIN_RESET) {
    printf("I2C1 Bus recovery: SDA held low by sensor. Toggling SCL to clear bus...\r\n");
    for (int i = 0; i < 9; i++) {
      HAL_GPIO_WritePin(TTEMP_SCL_GPIO_Port, TTEMP_SCL_Pin, GPIO_PIN_RESET);
      HAL_Delay(5);
      HAL_GPIO_WritePin(TTEMP_SCL_GPIO_Port, TTEMP_SCL_Pin, GPIO_PIN_SET);
      HAL_Delay(5);

      if (HAL_GPIO_ReadPin(TTEMP_SDA_GPIO_Port, TTEMP_SDA_Pin) == GPIO_PIN_SET) {
        printf("I2C1 Bus recovery: SDA released after %d clock pulses.\r\n", i + 1);
        break;
      }
    }
  }
}

static bool TryInitMLX90640(void) {
  // Clear the bus if SDA is stuck low before peripheral init
  I2C1_ClearBus();

  // Reset I2C peripheral to resolve any bus lockups
  HAL_I2C_DeInit(&hi2c1);
  HAL_Delay(5);
  MX_I2C1_Init();
  HAL_Delay(5);

  MLX90640_I2CInit(&hi2c1);

  if (HAL_I2C_IsDeviceReady(&hi2c1, (MLX90640_DEFAULT_SA << 1), 3, 100) != HAL_OK) {
    printf("MLX90640 not found on I2C bus.\r\n");
    return false;
  }

  int mlx_status = MLX90640_DumpEE(MLX90640_DEFAULT_SA, eeData);
  if (mlx_status != 0) {
    printf("MLX90640 EEPROM read failed: %d\r\n", mlx_status);
    return false;
  }

  mlx_status = MLX90640_ExtractParameters(eeData, &mlxParams);
  if (mlx_status != 0) {
    printf("MLX90640 parameter extraction failed: %d\r\n", mlx_status);
    return false;
  }

  MLX90640_SetRefreshRate(MLX90640_DEFAULT_SA, 0x05); // 16Hz
  MLX90640_SetResolution(MLX90640_DEFAULT_SA, 0x00);  // 16-bit
  MLX90640_SetChessMode(MLX90640_DEFAULT_SA);

  printf("MLX90640 initialized OK\r\n");
  return true;
}

static bool TryInitADC(void) {
  // Reset SPI peripheral to resolve any bus/DMA hangs
  HAL_SPI_DeInit(&hspi1);
  HAL_Delay(5);
  MX_SPI1_Init();
  HAL_Delay(5);

  HAL_StatusTypeDef status = MCP3464_Init(&hmcp3464);
  if (status != HAL_OK) {
    printf("MCP3464 reset/init failed: %d\r\n", status);
    return false;
  }

  status = MCP3464_SetMode(&hmcp3464, MCP3464_MODE_SCAN, MCP3464_MUX_CH0_AGND, MCP3464_SCAN_OSR);
  if (status != HAL_OK) {
    printf("MCP3464 mode scan set failed: %d\r\n", status);
    return false;
  }

  status = MCP3464_StartContinuous_DMA(&hmcp3464);
  if (status != HAL_OK) {
    printf("MCP3464 DMA start failed: %d\r\n", status);
    return false;
  }

  printf("MCP3464 ADC initialized OK\r\n");
  return true;
}

/* --- Private Signal Math Functions --- */
static void UpdateHotbarTempStats(const float *profile, SDU_State_t *state) {
  const int hotbarWidth = 32;
  float maxT = profile[0];
  float minT = profile[0];

  for (int i = 1; i < hotbarWidth; i++) {
    if (profile[i] > maxT)
      maxT = profile[i];
    if (profile[i] < minT)
      minT = profile[i];
  }

  state->tire_thermal.max_temp = maxT;
  state->tire_thermal.min_temp = minT;
  state->tire_thermal.center_temp = profile[hotbarWidth / 2];
}

static float ShockPotVoltageToTravel(float voltage) {
  float travel = SHOCK_POT_SLOPE * (voltage) - SHOCK_POT_OFFSET;
  return (travel > 0.0f) ? travel : 0.0f;
}

static void BuildTireProfile3RowAverage(const float *image, float *profile) {
  const int sensorWidth = 32;
  const int firstRow = 11;
  const int rowCount = 3;

  for (int col = 0; col < sensorWidth; col++) {
    float sum = 0.0f;

    for (int row = 0; row < rowCount; row++) {
      sum += image[(firstRow + row) * sensorWidth + col];
    }

    profile[col] = sum / (float)rowCount;
  }
}

static void MCP3464_Service(void) {
  uint32_t now_ms = HAL_GetTick();
  uint32_t elapsed_ms = now_ms - hmcp3464.sampleRateLastUpdateMs;

  if (elapsed_ms >= MCP3464_SAMPLE_RATE_WINDOW_MS) {
    uint32_t currentCounts[MCP3464_SCAN_CHANNEL_COUNT];

    __disable_irq();
    for (uint32_t channel = 0; channel < MCP3464_SCAN_CHANNEL_COUNT; channel++) {
      currentCounts[channel] = hmcp3464.channelSampleCount[channel];
    }
    __enable_irq();

    for (uint32_t channel = 0; channel < MCP3464_SCAN_CHANNEL_COUNT; channel++) {
      uint32_t delta = currentCounts[channel] - hmcp3464.sampleRateLastCount[channel];
      hmcp3464.channelSampleRate[channel] = ((float)delta * 1000.0f) / (float)elapsed_ms;
      hmcp3464.sampleRateLastCount[channel] = currentCounts[channel];
    }

    hmcp3464.sampleRateLastUpdateMs = now_ms;
  }

  if (!hmcp3464.dataReady)
    return;

  __disable_irq();
  hmcp3464.dataReady = 0U;
  hmcp3464.readyMask = 0U;
  __enable_irq();
}

/* --- API Implementations --- */

/**
 * @brief Initialize all sensor peripherals and onboard diagnostics.
 *        Configures wheel-speed capture, brake temperature, and the MLX90640 tire camera.
 * @return HAL_OK if initialization succeeds for supported devices.
 */
HAL_StatusTypeDef Telemetry_InitAllSensors(void) {

  // 1. OLED Screen initialization & boot splash
  SH1106_Init();
  SH1106_GotoXY(2, 0);
  SH1106_Puts("Initializing...", &Font_7x10, 1);
  SH1106_GotoXY(2, 12);
  SH1106_Puts("Corner PCB V4", &Font_7x10, 1);
  SH1106_DrawBitmap(2, 15, bfr_logo, 64, 64, 1);
  SH1106_UpdateScreenDMA();
  SH1106_Flush();
  HAL_Delay(1000);
  SH1106_Clear();

  // 2. Scan single-point IR brake temp sensor
  uint8_t btempID = MLX90614_ScanDevices();
  if (btempID > 0) {
    MLX90614_SetEmissivity(MLX90614_DEFAULT_SA, BRAKE_EMISSIVITY);
  }

  // 3. Start Wheel Speed timer capture
  HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_2);

  // 4. Test brake temp device ready
  SH1106_GotoXY(2, 0);
  SH1106_Puts("Init Brk Temp", &Font_7x10, 1);
  SH1106_UpdateScreenDMA();
  SH1106_Flush();
  
  uint8_t btempready = HAL_I2C_IsDeviceReady(&hi2c4, (MLX90614_DEFAULT_SA << 1), 3, 5);
  if (btempready == HAL_OK) {
    MLX90614_SetEmissivity(MLX90614_DEFAULT_SA, BRAKE_EMISSIVITY);
  }
  
  SH1106_GotoXY(2, 12);
  if (btempready == HAL_OK) {
    SH1106_Puts("Found!", &Font_7x10, 1);
  } else {
    SH1106_Puts("Not Found!", &Font_7x10, 1);
  }
  SH1106_UpdateScreenDMA();
  SH1106_Flush();
  HAL_Delay(750);
  SH1106_Clear();

  // 5. Initialize MLX90640 thermal matrix camera
  SH1106_GotoXY(2, 0);
  SH1106_Puts("Init Tire Temp", &Font_7x10, 1);
  SH1106_GotoXY(2, 12);
  SH1106_UpdateScreenDMA();

  if (TryInitMLX90640()) {
    mlx90640_detected = true;
    mlx_last_success_time_ms = HAL_GetTick();
    SH1106_Puts("Found OK!", &Font_7x10, 1);
  } else {
    mlx90640_detected = false;
    mlx_next_retry_time_ms = HAL_GetTick() + mlx_retry_interval_ms;
    SH1106_Puts("Not Found / Fail!", &Font_7x10, 1);
  }
  SH1106_UpdateScreenDMA();
  SH1106_Flush();
  HAL_Delay(500);
  SH1106_Clear();

  // 6. Initialize MCP3464 continuous DMA scan
  SH1106_GotoXY(2, 0);
  SH1106_Puts("Init ADC...", &Font_7x10, 1);
  SH1106_UpdateScreenDMA();

  hmcp3464.csPort = ADC_NSS_GPIO_Port;
  hmcp3464.csPin = ADC_NSS_Pin;
  hmcp3464.drdyPort = ADC_DRDY_IRQ_GPIO_Port;
  hmcp3464.drdyPin = ADC_DRDY_IRQ_Pin;
  hmcp3464.hspi = &hspi1;
  
  if (TryInitADC()) {
    adc_initialized = true;
    adc_last_success_time_ms = HAL_GetTick();
    SH1106_GotoXY(2, 12);
    SH1106_Puts("ADC OK!", &Font_5x8, 1);
  } else {
    adc_initialized = false;
    adc_next_retry_time_ms = HAL_GetTick() + adc_retry_interval_ms;
    SH1106_GotoXY(2, 12);
    SH1106_Puts("ADC FAIL!", &Font_5x8, 1);
  }
  SH1106_UpdateScreenDMA();
  SH1106_Flush();
  HAL_Delay(500);
  SH1106_Clear();

  // 7. Start Loop timing base timer
  HAL_TIM_Base_Start(&htim2);

  return HAL_OK;
}

/**
 * @brief Poll and process asynchronous sensor updates.
 *        Reads ADC, wheel speed, brake temperature, and matrix tire-frame data.
 * @param state Pointer to the shared SDU runtime state.
 */
void Telemetry_ProcessSensors(SDU_State_t *state) {
  uint32_t now = state->timestamp_ms;
  static uint32_t last_tire_process_time_ms = 0;

  // 1. Process external ADC scan services
  if (hmcp3464.dataReady) {
    adc_last_success_time_ms = now;
    adc_retry_interval_ms = 3000; // Reset backoff
  }

  if (adc_initialized) {
    // Check for ADC timeout
    if (now - adc_last_success_time_ms > ADC_TIMEOUT_MS) {
      printf("ADC Timeout! Disabling ADC and scheduling backoff.\r\n");
      adc_initialized = false;
      adc_next_retry_time_ms = now + adc_retry_interval_ms;
    }
  } else {
    // Check if it's time to retry ADC initialization
    if (now >= adc_next_retry_time_ms) {
      printf("Retrying ADC initialization...\r\n");
      if (TryInitADC()) {
        adc_initialized = true;
        adc_last_success_time_ms = now;
        adc_retry_interval_ms = 3000; // Reset backoff
      } else {
        adc_retry_interval_ms *= 2;
        if (adc_retry_interval_ms > ADC_MAX_RETRY_INTERVAL_MS) {
          adc_retry_interval_ms = ADC_MAX_RETRY_INTERVAL_MS;
        }
        adc_next_retry_time_ms = now + adc_retry_interval_ms;
      }
    }
  }

  MCP3464_Service();
  state->diagnostics.sampleCount = sampleCount;

  // 2. Fetch strain gauge raw voltages safely
  if (adc_initialized) {
    __disable_irq();
    for (uint32_t i = 0; i < 6; i++) {
      state->strain_gauge.voltages[i] = hmcp3464.scanVoltage[i];
    }
    float shockPotVoltage = hmcp3464.scanVoltage[SHOCK_POT_CHANNEL];
    __enable_irq();

    // 3. Process suspension travel displacement (Shock Pot)
    state->shock_pot.voltage = shockPotVoltage;
    state->shock_pot.travel_mm = ShockPotVoltageToTravel(shockPotVoltage);
  } else {
    // Zero out raw sensor readings if not initialized
    for (uint32_t i = 0; i < 6; i++) {
      state->strain_gauge.voltages[i] = 0.0f;
    }
    state->shock_pot.voltage = 0.0f;
    state->shock_pot.travel_mm = 0.0f;
  }

  // 4. Process wheel capture edge times and smooth RPM
  uint32_t last_ts;
  uint8_t data_ready;
  float freq;
  uint32_t trigger_count;
  
  __disable_irq();
  last_ts = isr_last_interrupt_timestamp;
  data_ready = isr_whs_data_ready;
  freq = isr_input_freq;
  trigger_count = isr_whs_trigger_count;
  isr_whs_data_ready = 0U; // Reset data ready flag
  __enable_irq();

  state->wheel_speed.input_freq = freq;
  state->wheel_speed.last_interrupt_timestamp = last_ts;
  state->wheel_speed.trigger_count = trigger_count;

  if ((now - last_ts) > WHEEL_SPEED_TIMEOUT_MS) {
    state->wheel_speed.input_freq = 0.0f;
    state->wheel_speed.measured_rpm = 0.0f;
    state->wheel_speed.data_ready = false;
  } else if (data_ready) {
    float raw_rpm = (freq * 60.0f) / BOLTS_ON_HUB;
    state->wheel_speed.measured_rpm = (raw_rpm * WHEEL_SPEED_FILTER_ALPHA) + (state->wheel_speed.measured_rpm * (1.0f - WHEEL_SPEED_FILTER_ALPHA));
    state->wheel_speed.data_ready = true;
  }

  // 5. Periodic Brake Rotor Temperature acquisition
  static uint32_t last_btemp_time = 0;
  if ((now - last_btemp_time) >= SCREEN_REFRESH_MS) {
    last_btemp_time = now;
    
    uint32_t temp_start = __HAL_TIM_GET_COUNTER(&htim2);
    
    state->brake_thermal.brake_temp_c = MLX90614_ReadTemp(MLX90614_DEFAULT_SA, MLX90614_TOBJ1);
    state->brake_thermal.ambient_temp_c = MLX90614_ReadTemp(MLX90614_DEFAULT_SA, MLX90614_TAMB);
    state->brake_thermal.sensor_ready = true;
    
    state->diagnostics.brake_sensor_time_us = __HAL_TIM_GET_COUNTER(&htim2) - temp_start;
  }

  // 6. Non-blocking Matrix Tire Thermal Camera step and calculations
  if (mlx90640_detected) {
    // Check for runtime camera timeout
    if (now - mlx_last_success_time_ms > MLX_TIMEOUT_MS) {
      printf("Tire temp camera timeout! Disabling camera and scheduling backoff.\r\n");
      mlx90640_detected = false;
      MLX90640_ResetDMAState();
      mlx_next_retry_time_ms = now + mlx_retry_interval_ms;
    } else {
      // Normal camera processing
      if (mlx_frame_ready == 0){
        uint32_t tire_start = __HAL_TIM_GET_COUNTER(&htim2);
        if (MLX90640_StepFrameDMA(0x33, frameData)) {
          mlx_frame_ready = 1;
        }
        state->diagnostics.tire_step_time_us = __HAL_TIM_GET_COUNTER(&htim2) - tire_start;
      }

      if (mlx_frame_ready && (now - last_tire_process_time_ms) >= 50U) {
        last_tire_process_time_ms = now;
        mlx_last_success_time_ms = now;
        mlx_retry_interval_ms = 3000; // Reset backoff

        float tireProfile[32];
        float reflectedTemp;

        uint32_t tire_start = __HAL_TIM_GET_COUNTER(&htim2);
        state->tire_thermal.ambient = MLX90640_GetTa(frameData, &mlxParams);
        state->diagnostics.tire_ta_time_us = __HAL_TIM_GET_COUNTER(&htim2) - tire_start;

        reflectedTemp = state->tire_thermal.ambient - TA_SHIFT;

        uint32_t calcStart = __HAL_TIM_GET_COUNTER(&htim2);
        MLX90640_CalculateTo_Hotbar(frameData, &mlxParams, TIRE_EMISSIVITY, reflectedTemp, tempImage);
        state->diagnostics.tire_calc_time_us = __HAL_TIM_GET_COUNTER(&htim2) - calcStart;

        BuildTireProfile3RowAverage(tempImage, tireProfile);

        tire_start = __HAL_TIM_GET_COUNTER(&htim2);
        UpdateHotbarTempStats(tireProfile, state);
        state->diagnostics.tire_hotbar_time_us = __HAL_TIM_GET_COUNTER(&htim2) - tire_start;

        memcpy(state->tire_thermal.profile, tireProfile, sizeof(tireProfile));
        state->tire_thermal.status = 0;
        state->tire_thermal.frame_ready = true;
        mlx_frame_ready = 0U;
      }
    }
  } else {
    // Check if it's time to retry MLX90640 initialization
    if (now >= mlx_next_retry_time_ms) {
      printf("Retrying Tire Temp camera initialization...\r\n");
      if (TryInitMLX90640()) {
        mlx90640_detected = true;
        mlx_last_success_time_ms = now;
        mlx_retry_interval_ms = 3000; // Reset backoff
      } else {
        mlx_retry_interval_ms *= 2;
        if (mlx_retry_interval_ms > MLX_MAX_RETRY_INTERVAL_MS) {
          mlx_retry_interval_ms = MLX_MAX_RETRY_INTERVAL_MS;
        }
        mlx_next_retry_time_ms = now + mlx_retry_interval_ms;
      }
    }
    // Zero out profile and temperatures if not present
    memset(state->tire_thermal.profile, 0, sizeof(state->tire_thermal.profile));
    state->tire_thermal.max_temp = 0.0f;
    state->tire_thermal.min_temp = 0.0f;
    state->tire_thermal.center_temp = 0.0f;
    state->tire_thermal.ambient = 0.0f;
  }
}

/* --- ISR Callback Forwarding Vectors --- */

/**
 * @brief Forward external interrupt events for the MCP3464 ADC data-ready signal.
 * @param pin GPIO pin that triggered the interrupt.
 */
void Telemetry_HandleGPIO_EXTI(uint16_t pin) {
  if (pin == ADC_DRDY_IRQ_Pin) {
    hmcp3464.extiCount++;
    MCP3464_ArmRead(&hmcp3464);
  }
}

/**
 * @brief Forward SPI transfer-complete callbacks for the MCP3464 ADC interface.
 * @param hspi SPI interface handle.
 */
void Telemetry_HandleSPI_TxRxCplt(SPI_HandleTypeDef *hspi) {
  if (hspi->Instance == SPI1) {
    MCP3464_TxRxCpltCallback(&hmcp3464);
  }
}

/**
 * @brief Forward SPI error callbacks for the MCP3464 ADC interface.
 * @param hspi SPI interface handle.
 */
void Telemetry_HandleSPI_Error(SPI_HandleTypeDef *hspi) {
  if (hspi->Instance == SPI1) {
    MCP3464_ErrorCallback(&hmcp3464);
  }
}

/**
 * @brief Handle wheel-speed input capture events from TIM1 channel 2.
 * @param htim Timer interface handle.
 */
void Telemetry_HandleTIM_IC_Capture(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
    isr_current_capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
    uint32_t now = HAL_GetTick();

    if (isr_first_capture == 0) {
      isr_last_capture = isr_current_capture;
      isr_first_capture = 1;
      isr_last_interrupt_timestamp = now;
    } else {
      uint32_t diff = 0;
      if (isr_current_capture >= isr_last_capture) {
        diff = isr_current_capture - isr_last_capture;
      } else {
        diff = (65535U - isr_last_capture) + isr_current_capture;
      }

      if (diff > 1000U) {
        isr_diff_capture = diff;
        isr_input_freq = 100000.0f / (float)isr_diff_capture;
        isr_last_interrupt_timestamp = now;
        isr_whs_data_ready = 1U;
        isr_whs_trigger_count++;
      }
      isr_last_capture = isr_current_capture;
    }
  }
}

/**
 * @brief Handle completed I2C memory reads and dispatch them to the MLX90640 DMA helper.
 * @param hi2c I2C interface handle.
 */
void Telemetry_HandleI2C_MemRxCplt(I2C_HandleTypeDef *hi2c) {
  if (MLX90640_HandleMemRxCplt(hi2c)) {
    return;
  }
}

#include "can_messaging.h"
#include "fdcan.h"
#include <stddef.h>
#include <string.h>


/* --- External Peripheral Handles --- */
extern TIM_HandleTypeDef htim2;
/* Board ID is set in main.c; we reference it here so CAN_Messaging_Init()
 * can compute the correct CAN ID base address for this physical board. */
extern uint8_t SDU_BOARD_ID;

/* --- New 8-Bit Uncompressed Tire Frame Definitions --- */
typedef struct {
  uint8_t max_temp;
  uint8_t min_temp;
  uint8_t center_temp;
  uint8_t ambient;
  int8_t jitter_ms;
} __attribute__((packed)) TireHistoryBlock_t;

typedef struct {
  uint32_t base_timestamp;
  uint16_t error_flags;
  TireHistoryBlock_t blocks[11]; // 11 historical snapshots * 5 bytes = 55 bytes
  uint8_t padding[3];            // 64 - 6 - 55 = 3 structural padding bytes
} __attribute__((packed)) TireHistoryFDFrame_t;

/* --- Private Network Config Headers --- */
static FDCAN_TxHeaderTypeDef canFastTxHeader;
static FDCAN_TxHeaderTypeDef canSlowTxHeader;
static FDCAN_TxHeaderTypeDef canThermalTxHeader;

/* --- Dedicated Transmit Headers --- */
static FDCAN_TxHeaderTypeDef canSGTxHeader;
static FDCAN_TxHeaderTypeDef canShockTxHeader;
static FDCAN_TxHeaderTypeDef canWheelTxHeader;
static FDCAN_TxHeaderTypeDef canBrakeTxHeader;
static FDCAN_TxHeaderTypeDef canTireTxHeader;

volatile uint32_t fdcan_rx_count = 0U;
extern volatile uint32_t fdcan_tx_count;

static bool can_tx_buffer_overflow = false;
static bool can_tx_error_overflow = false;
static bool can_rx_buffer_overflow = false;
static bool can_rx_error_overflow = false;
static uint8_t can_message_counter = 0;
static uint32_t can_last_timer_us = 0;

/* --- Buffered Outgoing/Incoming Telemetry Frames --- */
Fdcan64ByteFrame_t can_fast_data = {0};
Fdcan64ByteFrame_t can_slow_data = {0};
FdcanThermalFrame_t can_thermal_data = {0};
Fdcan64ByteFrame_t rx_fast_data = {0};
Fdcan64ByteFrame_t rx_slow_data = {0};

/* --- Packed Telemetry Payload Structs --- */
static StrainGaugeFDFrame_t sg_tx_frame;
static SensorFDFrame_t shock_tx_frame;
static SensorFDFrame_t wheel_tx_frame;
static SensorFDFrame_t brake_tx_frame;
static TireHistoryFDFrame_t tire_tx_frame;

/* --- Sampling State Variables --- */
// 1. Strain Gauges
static uint8_t sg_sample_idx = 0;
static uint32_t sg_last_sample_us = 0;
static uint32_t sg_base_timestamp_us = 0;

// 2. Shock Pot
static uint8_t shock_sample_idx = 0;
static uint32_t shock_last_sample_us = 0;
static uint32_t shock_base_timestamp_us = 0;

// 3. Wheel Speed (RPM)
static uint8_t wheel_sample_idx = 0;
static uint32_t wheel_last_sample_us = 0;
static uint32_t wheel_base_timestamp_us = 0;

// 4. Brake Temp
static uint8_t brake_sample_idx = 0;
static uint32_t brake_last_sample_us = 0;
static uint32_t brake_base_timestamp_us = 0;

// 5. Tire Temp
static uint8_t tire_sample_idx = 0;
static uint32_t tire_last_sample_us = 0;
static uint32_t tire_base_timestamp_us = 0;

/* --- ISR Telemetry Receive Registers --- */
static FDCAN_RxHeaderTypeDef rx_test_header;
static volatile uint8_t isr_rx_test_buffer[64] = {0};
static volatile uint32_t isr_rx_msg_received_count = 0;
static volatile uint32_t isr_rx_last_id = 0;

/* --- Private Helpers --- */

// OOR / stale / misc threshold configuration.
// Edit these values here when you want to change the OOR or misc error
// thresholds.
static const float OOR_HIGH_SHOCK_MM = 250.0f;
static const float OOR_HIGH_WHEEL_RPM = 12000.0f;
static const float OOR_HIGH_BRAKE_TEMP_C = 500.0f;
static const float OOR_HIGH_TIRE_TEMP_C = 255.0f;
static const float OOR_LOW_SHOCK_MM = -20.0f;
static const float OOR_LOW_WHEEL_RPM = 0.0f;
static const float OOR_LOW_BRAKE_TEMP_C = -40.0f;
static const float OOR_LOW_TIRE_TEMP_C = -40.0f;
static const uint32_t MISC_ERROR_LOOP_US = 150000U; // if main loop takes longer than this, set misc error

/* --- Rolling legacy frame sequence counter --- */
static uint8_t slow_tx_seq = 0;

/* Map a float temperature (°C) into [0, 255] assuming range [-40, 215]. */
static inline uint8_t FloatTo8Bit(float temp_c) {
  float norm = (temp_c + 40.0f) / 255.0f;
  if (norm < 0.0f) norm = 0.0f;
  if (norm > 1.0f) norm = 1.0f;
  return (uint8_t)(norm * 255.0f);
}

static inline void PackTireHistoryBlock(TireHistoryBlock_t *block,
                                        const TireThermal_State_t *tire,
                                        int32_t deviation_ms) {
  if (deviation_ms > 127)
    deviation_ms = 127;
  if (deviation_ms < -128)
    deviation_ms = -128;

  block->max_temp = FloatTo8Bit(tire->max_temp);
  block->min_temp = FloatTo8Bit(tire->min_temp);
  block->center_temp = FloatTo8Bit(tire->center_temp);
  block->ambient = FloatTo8Bit(tire->ambient);
  block->jitter_ms = (int8_t)deviation_ms;
}

static uint16_t StrainGauge_VoltageTo12Bit(float voltage) {
  // Map -3.3V to +3.3V to [0, 4095]
  float norm = (voltage + 3.3f) / 6.6f;
  if (norm < 0.0f)
    norm = 0.0f;
  if (norm > 1.0f)
    norm = 1.0f;
  return (uint16_t)(norm * 4095.0f);
}

static void PackStrainGaugeChannels(StrainGaugeBlock_t *block, uint16_t *vals) {
  block->ch1_upper = (uint8_t)((vals[0] >> 4) & 0xFF);
  block->ch2_upper = (uint8_t)((vals[1] >> 4) & 0xFF);
  block->ch1_ch2_lower = (uint8_t)(((vals[0] & 0x0F) << 4) | (vals[1] & 0x0F));

  block->ch3_upper = (uint8_t)((vals[2] >> 4) & 0xFF);
  block->ch4_upper = (uint8_t)((vals[3] >> 4) & 0xFF);
  block->ch3_ch4_lower = (uint8_t)(((vals[2] & 0x0F) << 4) | (vals[3] & 0x0F));

  block->ch5_upper = (uint8_t)((vals[4] >> 4) & 0xFF);
  block->ch6_upper = (uint8_t)((vals[5] >> 4) & 0xFF);
  block->ch5_ch6_lower = (uint8_t)(((vals[4] & 0x0F) << 4) | (vals[5] & 0x0F));
}

/* --- API Implementations --- */

static inline bool IsWheelSpeedStale(const SDU_State_t *state) {
  if (state == NULL) {
    return false;
  }
  // Wheel speed has an explicit timestamp field, so it is the only sensor with
  // a direct stale-data check in the current state model. Additional stale
  // thresholds can be added once more sensors expose their own last-update time
  // fields.
  return ((state->timestamp_ms - state->wheel_speed.last_interrupt_timestamp) >
          WHEEL_SPEED_TIMEOUT_MS);
}

static inline bool IsEnvironmentWarning(const SDU_State_t *state) {
  if (state == NULL) {
    return false;
  }
  // Simple thermal warning if sensor ambient temperatures exceed expected
  // vehicle limits.
  return (state->tire_thermal.ambient > 120.0f ||
          state->brake_thermal.ambient_temp_c > 120.0f);
}

static const uint32_t STALE_BRAKE_US = 300000U; // 300 ms
static const uint32_t STALE_TIRE_US = 300000U;  // 300 ms

static inline bool IsDataOutOfRangeHigh(const SDU_State_t *state) {
  if (state == NULL) {
    return false;
  }
  if (state->shock_pot.travel_mm > OOR_HIGH_SHOCK_MM) {
    return true;
  }
  if (state->wheel_speed.measured_rpm > OOR_HIGH_WHEEL_RPM) {
    return true;
  }
  if (state->brake_thermal.brake_temp_c > OOR_HIGH_BRAKE_TEMP_C) {
    return true;
  }
  if (state->tire_thermal.max_temp > OOR_HIGH_TIRE_TEMP_C ||
      state->tire_thermal.center_temp > OOR_HIGH_TIRE_TEMP_C ||
      state->tire_thermal.ambient > OOR_HIGH_TIRE_TEMP_C) {
    return true;
  }
  return false;
}

static inline bool IsDataOutOfRangeLow(const SDU_State_t *state) {
  if (state == NULL) {
    return false;
  }
  if (state->shock_pot.travel_mm < OOR_LOW_SHOCK_MM) {
    return true;
  }
  if (state->wheel_speed.measured_rpm < OOR_LOW_WHEEL_RPM) {
    return true;
  }
  if (state->brake_thermal.brake_temp_c < OOR_LOW_BRAKE_TEMP_C) {
    return true;
  }
  if (state->tire_thermal.min_temp < OOR_LOW_TIRE_TEMP_C ||
      state->tire_thermal.center_temp < OOR_LOW_TIRE_TEMP_C ||
      state->tire_thermal.ambient < OOR_LOW_TIRE_TEMP_C) {
    return true;
  }
  return false;
}

static inline bool IsBrakeTempStale(uint32_t now_us) {
  return ((now_us - brake_last_sample_us) > STALE_BRAKE_US);
}

static inline bool IsTireTempStale(uint32_t now_us) {
  return ((now_us - tire_last_sample_us) > STALE_TIRE_US);
}

static uint16_t BuildCommonErrorFlags(const SDU_State_t *state,
                                      bool timeOverflow, bool jitterOverflow,
                                      bool sensorValid, bool staleData) {
  uint16_t flags = 0;

  if (timeOverflow) {
    flags |= CAN_ERROR_FLAG_TIME_OVERFLOW;
  }
  if (jitterOverflow) {
    flags |= CAN_ERROR_FLAG_JITTER_OVERFLOW;
  }
  if (!sensorValid) {
    flags |= CAN_ERROR_FLAG_SENSOR_VALIDITY;
  }
  if (can_tx_buffer_overflow) {
    flags |= CAN_ERROR_FLAG_CAN_TX_BUFFER_OVERFLOW;
  }
  if (can_tx_error_overflow) {
    flags |= CAN_ERROR_FLAG_CAN_TX_ERROR_OVERFLOW;
  }
  if (can_rx_buffer_overflow) {
    flags |= CAN_ERROR_FLAG_CAN_RX_BUFFER_OVERFLOW;
  }
  if (can_rx_error_overflow) {
    flags |= CAN_ERROR_FLAG_CAN_RX_ERROR_OVERFLOW;
  }

  if (staleData) {
    flags |= CAN_ERROR_FLAG_STALE_DATA;
  }
  // Hardware fault is currently driven by explicit hardware status fields only.
  // If you want to cover more sensors, add additional status indicators to
  // SDU_State_t.
  if (state != NULL &&
      (!state->brake_thermal.sensor_ready || state->tire_thermal.status != 0)) {
    flags |= CAN_ERROR_FLAG_HARDWARE_FAULT;
  }
  if (IsEnvironmentWarning(state)) {
    flags |= CAN_ERROR_FLAG_ENVIRONMENT_WARNING;
  }
  if (state != NULL && IsDataOutOfRangeHigh(state)) {
    flags |= CAN_ERROR_FLAG_OORH;
  }
  if (state != NULL && IsDataOutOfRangeLow(state)) {
    flags |= CAN_ERROR_FLAG_OORL;
  }
  if (state != NULL &&
      state->diagnostics.loop_duration_us > MISC_ERROR_LOOP_US) {
    flags |= CAN_ERROR_FLAG_MISC_ERROR; // set when the main loop is very slow;
                                        // adjust MISC_ERROR_LOOP_US above
  }

  // Bits 7-9 are a rolling 3-bit counter, so the reader should extract it as:
  //   (error_flags >> CAN_ERROR_FLAG_MESSAGE_COUNTER_SHIFT) & 0x07U
  flags |= (uint16_t)((can_message_counter & 0x07U)
                      << CAN_ERROR_FLAG_MESSAGE_COUNTER_SHIFT);

  return flags;
}

/**
 * @brief Initialize FDCAN transmit headers and start the microsecond base
 * timer. Sets up the SDU fast and slow frame identifiers and prepares packet
 * timing.
 */
void CAN_Messaging_Init(void) {
  // Configure common headers settings
  FDCAN_TxHeaderTypeDef commonHeader;
  commonHeader.IdType = FDCAN_STANDARD_ID;
  commonHeader.TxFrameType = FDCAN_DATA_FRAME;
  commonHeader.DataLength = FDCAN_DLC_BYTES_64;
  commonHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  commonHeader.BitRateSwitch = FDCAN_BRS_ON;
  commonHeader.FDFormat = FDCAN_FD_CAN;
  commonHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  commonHeader.MessageMarker = 0;

  /* SDU CAN ID Bit-Packing Implementation Details:
   * According to the spreadsheet, standard 11-bit CAN IDs are formatted as follows:
   *   - Bits 9-6 (4 bits)   = Board Type: SDU is 2 (binary 0010). Shifting left by 6 results in base 0x080.
   *   - Bits 5-3 (3 bits)   = Number Board (Board Index): Configured in main.c (0 to 3). Shifting left by 3
   *                           results in a board-specific block spacing of 8 per board:
   *                             - FL SDU (Index 0) -> sdu_base = 0x080
   *                             - FR SDU (Index 1) -> sdu_base = 0x088
   *                             - RL SDU (Index 2) -> sdu_base = 0x090
   *                             - RR SDU (Index 3) -> sdu_base = 0x098
   *   - Bits 2-0 (3 bits)   = Board Sensor Number: Identifies the sensor index directly at the LSB (0 to 4).
   *
   * By changing SDU_BOARD_ID in main.c, this formula dynamically shifts the entire block of sensor IDs
   * to target the chosen board's hex range (e.g. 0x080-0x087, 0x088-0x08F, etc.) cleanly and without overlap.
   */
  uint32_t sdu_base = 0x80U | ((uint32_t)SDU_BOARD_ID << 3);

  // Strain Gauge (Sensor Index 0) -> Fast Task 200Hz
  canSGTxHeader = commonHeader;
  canSGTxHeader.Identifier = sdu_base | 0U; // e.g. FL = 0x080, FR = 0x088

  // Shock Pot (Sensor Index 1) -> Fast Task 200Hz
  canShockTxHeader = commonHeader;
  canShockTxHeader.Identifier = sdu_base | 1U; // e.g. FL = 0x081, FR = 0x089

  // Wheel Speed (Sensor Index 4) -> Slow Task 10Hz
  canWheelTxHeader = commonHeader;
  canWheelTxHeader.Identifier = sdu_base | 4U; // e.g. FL = 0x084, FR = 0x08C

  // Brake Temp (Sensor Index 2) -> Slow Task 10Hz
  canBrakeTxHeader = commonHeader;
  canBrakeTxHeader.Identifier = sdu_base | 2U; // e.g. FL = 0x082, FR = 0x08A

  // Tire Temp (Sensor Index 3) -> Slow Task 10Hz
  canTireTxHeader = commonHeader;
  canTireTxHeader.Identifier = sdu_base | 3U; // e.g. FL = 0x083, FR = 0x08B

  // Thermal Frame -> Paced by frame_ready
  canThermalTxHeader = commonHeader;
  canThermalTxHeader.Identifier = CAN_THERMAL_ID;


  // Keep compatibility for canFastTxHeader / canSlowTxHeader
  canFastTxHeader = commonHeader;
  canFastTxHeader.Identifier = CAN_FAST_ID;
  canFastTxHeader.IdType = FDCAN_STANDARD_ID;
  canFastTxHeader.TxFrameType = FDCAN_DATA_FRAME;
  canFastTxHeader.DataLength = FDCAN_DLC_BYTES_64;
  canFastTxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  canFastTxHeader.BitRateSwitch = FDCAN_BRS_OFF;
  canFastTxHeader.FDFormat = FDCAN_FD_CAN;
  canFastTxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  canFastTxHeader.MessageMarker = 0;

  // 2. Configure the 10Hz Slow Transmission Header
  canSlowTxHeader.Identifier = CAN_SLOW_ID;
  canSlowTxHeader.IdType = FDCAN_STANDARD_ID;
  canSlowTxHeader.TxFrameType = FDCAN_DATA_FRAME;
  canSlowTxHeader.DataLength = FDCAN_DLC_BYTES_64;
  canSlowTxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  canSlowTxHeader.BitRateSwitch = FDCAN_BRS_OFF;
  canSlowTxHeader.FDFormat = FDCAN_FD_CAN;
  canSlowTxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  canSlowTxHeader.MessageMarker = 0;

  // Initialize microsecond base timers
  HAL_TIM_Base_Start(&htim2);
  uint32_t startup_us = __HAL_TIM_GET_COUNTER(&htim2);
  sg_last_sample_us = startup_us;
  sg_base_timestamp_us = startup_us;

  shock_last_sample_us = startup_us;
  shock_base_timestamp_us = startup_us;

  wheel_last_sample_us = startup_us;
  wheel_base_timestamp_us = startup_us;

  brake_last_sample_us = startup_us;
  brake_base_timestamp_us = startup_us;

  tire_last_sample_us = startup_us;
  tire_base_timestamp_us = startup_us;
  can_last_timer_us = startup_us;
  can_tx_buffer_overflow = false;
  can_tx_error_overflow = false;
  can_rx_buffer_overflow = false;
  can_rx_error_overflow = false;
  can_message_counter = 0;
}

/**
 * @brief Sample sensor state and assemble batched FDCAN payloads.
 *        Fast blocks are collected every 5ms; slow blocks are collected every
 * 100ms. Tire temperature history is sampled every 100ms and transmitted after
 * 11 blocks.
 * @param state Pointer to the shared SDU runtime state structure.
 */
void CAN_Messaging_ProcessTransmissions(SDU_State_t *state) {
  uint32_t now = state->timestamp_ms;
  uint32_t now_us = __HAL_TIM_GET_COUNTER(&htim2);
  (void)now; /* reserved for ms-domain scheduling; currently all paths use now_us */


  // ==========================================
  // 1. STRAIN GAUGE SAMPLING (Every 5000us / 5 ms)
  // ==========================================
  const uint32_t SG_PERIOD_US = 5000U;
  if (now_us - sg_last_sample_us >= SG_PERIOD_US) {
    uint32_t elapsed_us = now_us - sg_last_sample_us;
    sg_last_sample_us = now_us;

    if (sg_sample_idx < 5) {
      uint16_t vals[6];
      for (int i = 0; i < 6; i++) {
        vals[i] = StrainGauge_VoltageTo12Bit(state->strain_gauge.voltages[i]);
      }

      int32_t jitter = (int32_t)elapsed_us - (int32_t)SG_PERIOD_US;
      if (jitter > 127)
        jitter = 127;
      if (jitter < -128)
        jitter = -128;

      PackStrainGaugeChannels(&sg_tx_frame.blocks[sg_sample_idx], vals);
      sg_tx_frame.blocks[sg_sample_idx].jitter_us = (int8_t)jitter;
      sg_sample_idx++;
    }
  }

  // ==========================================
  // 2. SHOCK POT SAMPLING (Every 5000us / 5 ms)
  // ==========================================
  const uint32_t SHOCK_PERIOD_US = 5000U;
  if (now_us - shock_last_sample_us >= SHOCK_PERIOD_US) {
    uint32_t elapsed_us = now_us - shock_last_sample_us;
    shock_last_sample_us = now_us;

    if (shock_sample_idx < 19) {
      int32_t jitter = (int32_t)elapsed_us - (int32_t)SHOCK_PERIOD_US;
      if (jitter > 127)
        jitter = 127;
      if (jitter < -128)
        jitter = -128;

      shock_tx_frame.samples[shock_sample_idx].value =
          (uint16_t)(state->shock_pot.travel_mm * 100.0f);
      shock_tx_frame.samples[shock_sample_idx].jitter_us = (int8_t)jitter;
      shock_sample_idx++;
    }
  }

  // ==========================================
  // 3. SLOW SENSORS SAMPLING (Every 100000us / 100 ms)
  // ==========================================
  const uint32_t SLOW_PERIOD_US = 100000U;

  // A. Wheel Speed
  if (now_us - wheel_last_sample_us >= SLOW_PERIOD_US) {
    uint32_t elapsed_us = now_us - wheel_last_sample_us;
    wheel_last_sample_us = now_us;

    if (wheel_sample_idx < 19) {
      int32_t jitter = (int32_t)elapsed_us - (int32_t)SLOW_PERIOD_US;
      if (jitter > 127)
        jitter = 127;
      if (jitter < -128)
        jitter = -128;

      wheel_tx_frame.samples[wheel_sample_idx].value =
          (uint16_t)(state->wheel_speed.measured_rpm * 10.0f);
      wheel_tx_frame.samples[wheel_sample_idx].jitter_us = (int8_t)jitter;
      wheel_sample_idx++;
    }
  }

  // B. Brake Temp
  if (now_us - brake_last_sample_us >= SLOW_PERIOD_US) {
    uint32_t elapsed_us = now_us - brake_last_sample_us;
    brake_last_sample_us = now_us;

    if (brake_sample_idx < 19) {
      int32_t jitter = (int32_t)elapsed_us - (int32_t)SLOW_PERIOD_US;
      if (jitter > 127)
        jitter = 127;
      if (jitter < -128)
        jitter = -128;

      brake_tx_frame.samples[brake_sample_idx].value =
          (uint16_t)(state->brake_thermal.brake_temp_c * 10.0f);
      brake_tx_frame.samples[brake_sample_idx].jitter_us = (int8_t)jitter;
      brake_sample_idx++;
    }
  }

  // C. Tire Temp (sample every 100 ms, batch 11 blocks, then send)
  // This mirrors the paced sampler pattern used by other sensors.
  const uint32_t TIRE_SAMPLE_PERIOD_US = 100000U;
  if (now_us - tire_last_sample_us >= TIRE_SAMPLE_PERIOD_US) {
    uint32_t elapsed_us = now_us - tire_last_sample_us;
    tire_last_sample_us = now_us;

    if (tire_sample_idx == 0) {
      tire_base_timestamp_us = now_us;
    }

    int32_t deviation_ms =
        ((int32_t)elapsed_us - (int32_t)TIRE_SAMPLE_PERIOD_US) / 1000;

    if (tire_sample_idx < 11) {
      PackTireHistoryBlock(&tire_tx_frame.blocks[tire_sample_idx],
                           &state->tire_thermal, deviation_ms);
      tire_sample_idx++;
    }
  }

  // ==========================================
  // 4. TRANSMISSION SCHEDULER
  // ==========================================
  bool stale_data = IsWheelSpeedStale(state) || IsBrakeTempStale(now_us) ||
                    IsTireTempStale(now_us);
  // Fast Telemetry Frame Send (Every 25ms / 40 Hz)
  if (sg_sample_idx >= 5) {
    sg_tx_frame.base_timestamp = sg_base_timestamp_us;
    bool time_overflow = now_us < can_last_timer_us;
    can_last_timer_us = now_us;
    bool jitter_overflow = false;
    for (int i = 0; i < 5; ++i) {
      int32_t sample_jitter = sg_tx_frame.blocks[i].jitter_us;
      if (sample_jitter == 127 || sample_jitter == -128) {
        jitter_overflow = true;
        break;
      }
    }

    bool sensor_valid = true;
    uint16_t error_flags = BuildCommonErrorFlags(
        state, time_overflow, jitter_overflow, sensor_valid, stale_data);
    sg_tx_frame.error_flags = error_flags;
    memset(sg_tx_frame.padding, 0, sizeof(sg_tx_frame.padding));

    uint32_t can_start = __HAL_TIM_GET_COUNTER(&htim2);
    HAL_StatusTypeDef status =
        CAN_SendPacked64BytePayload(&canSGTxHeader, &sg_tx_frame);
    if (status != HAL_OK) {
      can_tx_buffer_overflow = true;
      can_tx_error_overflow = true;
    }
    state->diagnostics.can_send_time_us =
        __HAL_TIM_GET_COUNTER(&htim2) - can_start;
    can_message_counter = (can_message_counter + 1) & 0x07U;

    // Reset buffer tracking
    sg_sample_idx = 0;
    sg_base_timestamp_us = now_us;
  }

  if (shock_sample_idx >= 19) {
    shock_tx_frame.base_timestamp = shock_base_timestamp_us;
    bool time_overflow = now_us < can_last_timer_us;
    can_last_timer_us = now_us;
    bool jitter_overflow = false;
    for (int i = 0; i < 19; ++i) {
      int32_t sample_jitter = shock_tx_frame.samples[i].jitter_us;
      if (sample_jitter == 127 || sample_jitter == -128) {
        jitter_overflow = true;
        break;
      }
    }

    bool sensor_valid = true;
    uint16_t error_flags = BuildCommonErrorFlags(
        state, time_overflow, jitter_overflow, sensor_valid, stale_data);
    shock_tx_frame.error_flags = error_flags;

    memset(((uint8_t *)&shock_tx_frame) + offsetof(SensorFDFrame_t, padding), 0,
           sizeof(shock_tx_frame.padding));

    HAL_StatusTypeDef status =
        CAN_SendPacked64BytePayload(&canShockTxHeader, &shock_tx_frame);
    if (status != HAL_OK) {
      can_tx_buffer_overflow = true;
      can_tx_error_overflow = true;
    }
    can_message_counter = (can_message_counter + 1) & 0x07U;

    shock_sample_idx = 0;
    shock_base_timestamp_us = now_us;
  }

  // Wheel Speed Transmission (every 19 samples * 100ms = ~1.9s)
  if (wheel_sample_idx >= 19) {
    wheel_tx_frame.base_timestamp = wheel_base_timestamp_us;
    bool time_overflow = now_us < can_last_timer_us;
    can_last_timer_us = now_us;
    bool jitter_overflow = false;
    for (int i = 0; i < 19; ++i) {
      int32_t sample_jitter = wheel_tx_frame.samples[i].jitter_us;
      if (sample_jitter == 127 || sample_jitter == -128) {
        jitter_overflow = true;
        break;
      }
    }

    bool sensor_valid = !IsWheelSpeedStale(state);
    uint16_t error_flags = BuildCommonErrorFlags(
        state, time_overflow, jitter_overflow, sensor_valid, stale_data);
    wheel_tx_frame.error_flags = error_flags;
    memset(((uint8_t *)&wheel_tx_frame) + offsetof(SensorFDFrame_t, padding), 0,
           sizeof(wheel_tx_frame.padding));

    HAL_StatusTypeDef status =
        CAN_SendPacked64BytePayload(&canWheelTxHeader, &wheel_tx_frame);
    if (status != HAL_OK) {
      can_tx_buffer_overflow = true;
      can_tx_error_overflow = true;
    }
    can_message_counter = (can_message_counter + 1) & 0x07U;

    wheel_sample_idx = 0;
    wheel_base_timestamp_us = now_us;
  }

  // Brake Temp Transmission (every 19 samples * 100ms = ~1.9s)
  if (brake_sample_idx >= 19) {
    brake_tx_frame.base_timestamp = brake_base_timestamp_us;
    bool time_overflow = now_us < can_last_timer_us;
    can_last_timer_us = now_us;
    bool jitter_overflow = false;
    for (int i = 0; i < 19; ++i) {
      int32_t sample_jitter = brake_tx_frame.samples[i].jitter_us;
      if (sample_jitter == 127 || sample_jitter == -128) {
        jitter_overflow = true;
        break;
      }
    }

    bool sensor_valid = state->brake_thermal.sensor_ready && !IsBrakeTempStale(now_us);
    uint16_t error_flags = BuildCommonErrorFlags(
        state, time_overflow, jitter_overflow, sensor_valid, stale_data);
    brake_tx_frame.error_flags = error_flags;
    memset(((uint8_t *)&brake_tx_frame) + offsetof(SensorFDFrame_t, padding), 0,
           sizeof(brake_tx_frame.padding));

    HAL_StatusTypeDef status =
        CAN_SendPacked64BytePayload(&canBrakeTxHeader, &brake_tx_frame);
    if (status != HAL_OK) {
      can_tx_buffer_overflow = true;
      can_tx_error_overflow = true;
    }
    can_message_counter = (can_message_counter + 1) & 0x07U;

    brake_sample_idx = 0;
    brake_base_timestamp_us = now_us;
  }

  // Tire Temp Transmission (every 11 blocks * 100ms = ~1.1s)
  if (tire_sample_idx >= 11) {
    tire_tx_frame.base_timestamp = tire_base_timestamp_us;
    bool time_overflow = now_us < can_last_timer_us;
    can_last_timer_us = now_us;

    bool sensor_valid = (state->tire_thermal.status == 0) && !IsTireTempStale(now_us);
    uint16_t error_flags = BuildCommonErrorFlags(
        state, time_overflow, false, sensor_valid, stale_data);
    tire_tx_frame.error_flags = error_flags;
    memset(tire_tx_frame.padding, 0, sizeof(tire_tx_frame.padding));

    HAL_StatusTypeDef status =
        CAN_SendPacked64BytePayload(&canTireTxHeader, &tire_tx_frame);
    if (status != HAL_OK) {
      can_tx_buffer_overflow = true;
      can_tx_error_overflow = true;
    }
    can_message_counter = (can_message_counter + 1) & 0x07U;

    tire_sample_idx = 0;
    tire_base_timestamp_us = now_us;
  }



  // --- Thermal Task: Triggered by new frame ---
  if (state->tire_thermal.frame_ready) {
    state->tire_thermal.frame_ready = false;
    for (int i = 0; i < 32; i++) {
        can_thermal_data.pixels[i] = (int16_t)(state->tire_thermal.profile[i] * 10.0f);
    }
    CAN_SendThermalFrame(&canThermalTxHeader, &can_thermal_data);
  }

  // --- Process RX Buffer Queues ---
  uint32_t rx_id;
  uint32_t rx_count;
  uint8_t rx_buf[64];

  __disable_irq();
  rx_id = isr_rx_last_id;
  rx_count = isr_rx_msg_received_count;
  memcpy(rx_buf, (void *)isr_rx_test_buffer, 64);
  isr_rx_last_id = 0;
  __enable_irq();

  if (rx_id != 0) {
    state->network.rx_last_id = rx_id;
    state->network.rx_msg_received_count = rx_count;
    memcpy(state->network.rx_test_buffer, rx_buf, 64);
  }

  // Stuck-transmission watchdog (1Hz periodic execution)
  static uint32_t last_watchdog_time_ms = 0;
  static uint8_t can_network_detected = 0U;
  static uint32_t last_tx_count = 0U;
  static uint32_t tx_stuck_seconds = 0U;

  if (now - last_watchdog_time_ms >= 1000) {
    last_watchdog_time_ms = now;

    if (!can_network_detected && fdcan_rx_count > 0U) {
      can_network_detected = 1U;
    }

    if (can_network_detected) {
      if (hfdcan1.Instance != NULL && hfdcan1.Instance->TXBRP != 0U && fdcan_tx_count == last_tx_count) {
        tx_stuck_seconds++;
        if (tx_stuck_seconds >= 5U) {
          printf("CAN Watchdog: CAN transmission stuck for 5s. Resetting system...\r\n");
          HAL_Delay(100);
          NVIC_SystemReset();
        }
      } else {
        tx_stuck_seconds = 0U;
        last_tx_count = fdcan_tx_count;
      }
    } else {
      last_tx_count = fdcan_tx_count;
    }

    /* Auto-recover from bus-off */
    FDCAN_ProtocolStatusTypeDef psr = {0};
    if (HAL_FDCAN_GetProtocolStatus(&hfdcan1, &psr) == HAL_OK) {
      if (psr.BusOff != 0U) {
        (void)HAL_FDCAN_Start(&hfdcan1);
      }
    }
  }
}

/* --- ISR Callback Forwarding Vectors --- */

/**
 * @brief Handle incoming FDCAN FIFO0 callbacks and cache the last received
 * 64-byte frame.
 * @param hfdcan FDCAN interface handle.
 * @param RxFifo0ITs Active FIFO0 interrupt flags.
 */
void CAN_Messaging_HandleRxFifoCallback(FDCAN_HandleTypeDef *hfdcan,
                                        uint32_t RxFifo0ITs) {
  if ((RxFifo0ITs & (FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO0_FULL |
                     FDCAN_IT_RX_FIFO0_MESSAGE_LOST)) != 0U) {
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_FULL) != 0U) {
      can_rx_buffer_overflow = true;
    }
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_MESSAGE_LOST) != 0U) {
      can_rx_error_overflow = true;
    }
    while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0) > 0) {
      uint8_t local_buf[64];
      if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rx_test_header,
                             local_buf) == HAL_OK) {
        fdcan_rx_count++;

        if (rx_test_header.Identifier == 0x100) {
          memcpy(&rx_fast_data, local_buf, 64);
        } else if (rx_test_header.Identifier == 0x200) {
          memcpy(&rx_slow_data, local_buf, 64);
        }

        // Buffer into the volatile variables for non-blocking read in the main
        // scheduler thread
        isr_rx_last_id = rx_test_header.Identifier;
        memcpy((void *)isr_rx_test_buffer, local_buf, 64);
        isr_rx_msg_received_count++;
      }
    }
  }
}

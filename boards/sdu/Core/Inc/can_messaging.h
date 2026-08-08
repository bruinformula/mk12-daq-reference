#ifndef CAN_MESSAGING_H
#define CAN_MESSAGING_H

#include "main.h"
#include "sdu_state.h"

// Shared CAN error flag definitions for the first 6 bytes of every SDU payload.
#define CAN_ERROR_FLAG_TIME_OVERFLOW           (1U << 0)
#define CAN_ERROR_FLAG_JITTER_OVERFLOW         (1U << 1)
#define CAN_ERROR_FLAG_SENSOR_VALIDITY         (1U << 2)
#define CAN_ERROR_FLAG_CAN_TX_BUFFER_OVERFLOW  (1U << 3)
#define CAN_ERROR_FLAG_CAN_TX_ERROR_OVERFLOW   (1U << 4)
#define CAN_ERROR_FLAG_CAN_RX_BUFFER_OVERFLOW  (1U << 5)
#define CAN_ERROR_FLAG_CAN_RX_ERROR_OVERFLOW   (1U << 6)
#define CAN_ERROR_FLAG_MESSAGE_COUNTER_MASK    (0x7U << 7)
#define CAN_ERROR_FLAG_MESSAGE_COUNTER_SHIFT   7U
#define CAN_ERROR_FLAG_OORH                    (1U << 10)
#define CAN_ERROR_FLAG_OORL                    (1U << 11)
#define CAN_ERROR_FLAG_STALE_DATA              (1U << 12)
#define CAN_ERROR_FLAG_HARDWARE_FAULT          (1U << 13)
#define CAN_ERROR_FLAG_ENVIRONMENT_WARNING     (1U << 14)
// Bit 15: Miscellaneous diagnostic error. Currently used when the main SDU
// loop takes longer than the configured threshold defined by
// MISC_ERROR_LOOP_US.
#define CAN_ERROR_FLAG_MISC_ERROR              (1U << 15)

// 1. Packed 6-Channel Strain Gauge Block (10 bytes)
typedef struct __attribute__((packed)) {
    uint8_t ch1_upper;          // Ch 1 Bits [11:4]
    uint8_t ch2_upper;          // Ch 2 Bits [11:4]
    uint8_t ch1_ch2_lower;      // Bits [7:4]: Ch 1 [3:0] | Bits [3:0]: Ch 2 [3:0]
    uint8_t ch3_upper;          // Ch 3 Bits [11:4]
    uint8_t ch4_upper;          // Ch 4 Bits [11:4]
    uint8_t ch3_ch4_lower;      // Bits [7:4]: Ch 3 [3:0] | Bits [3:0]: Ch 4 [3:0]
    uint8_t ch5_upper;          // Ch 5 Bits [11:4]
    uint8_t ch6_upper;          // Ch 6 Bits [11:4]
    uint8_t ch5_ch6_lower;      // Bits [7:4]: Ch 5 [3:0] | Bits [3:0]: Ch 6 [3:0]
    int8_t  jitter_us;          // us deviation from expected sample time
} StrainGaugeBlock_t;

// Complete 64-byte Strain Gauge FDCAN Payload
typedef struct __attribute__((packed)) {
    uint32_t base_timestamp;    // Bytes 0-3
    uint16_t error_flags;       // Bytes 4-5
    StrainGaugeBlock_t blocks[5]; // Bytes 6-55 (50 bytes)
    uint8_t  padding[8];        // Bytes 56-63
} StrainGaugeFDFrame_t;

// 2. Standard Sensor Sample Block (3 bytes)
typedef struct __attribute__((packed)) {
    uint16_t value;             // 2 bytes (raw or scaled * scalar)
    int8_t   jitter_us;         // 1 byte (us deviation)
} SensorSample_t;

// Complete 64-byte Standard Sensor FDCAN Payload
typedef struct __attribute__((packed)) {
    uint32_t base_timestamp;    // Bytes 0-3
    uint16_t error_flags;       // Bytes 4-5
    SensorSample_t samples[19]; // Bytes 6-62 (57 bytes)
    uint8_t  padding;           // Byte 63
} SensorFDFrame_t;


/* --- Initialization --- */
/**
 * @brief Pre-configures the FDCAN fast and slow transmit message headers.
 */
void CAN_Messaging_Init(void);

/* --- Main Pacing Pipeline --- */
/**
 * @brief Packages SDU sensor readouts and dispatches FDCAN broadcasts.
 *        Batches fast sensor blocks and slow telemetry blocks for consistent
 *        fixed timing across strain, shock, wheel, brake, and tire channels.
 * @param state Pointer to the master SDU state.
 */
void CAN_Messaging_ProcessTransmissions(SDU_State_t *state);

/* --- ISR Callback Forwarding Vectors --- */
/**
 * @brief Handles FDCAN Rx FIFO new message and overflow callback vectors.
 * @param hfdcan FDCAN bus handle.
 * @param RxFifo0ITs Active FIFO interrupt flags.
 */
void CAN_Messaging_HandleRxFifoCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs);

#endif /* CAN_MESSAGING_H */

#ifndef SENSOR_CALIB_CONSTANTS_H
#define SENSOR_CALIB_CONSTANTS_H

#include "mcp3464.h" // Required for OSR configurations

/* --- Board Configuration --- */
/* Board identity constants — set SDU_BOARD_ID to one of these in main.c
 * before flashing, so each physical SDU gets a unique CAN ID block. */
#define SDU_BOARD_FL  0U  /* Front-Left  -> CAN base 0x080 */
#define SDU_BOARD_FR  1U  /* Front-Right -> CAN base 0x088 */
#define SDU_BOARD_RL  2U  /* Rear-Left   -> CAN base 0x090 */
#define SDU_BOARD_RR  3U  /* Rear-Right  -> CAN base 0x098 */

/* --- Periodic Tasks Timing --- */
#define SCREEN_REFRESH_MS 250U
#define CAN_FAST_INTERVAL_MS 5U   // 200Hz
#define CAN_SLOW_INTERVAL_MS 100U // 10Hz
#define WHEEL_SPEED_TIMEOUT_MS 500U

/* --- Diagnostic Thresholds --- */
#define LAST_LOOP_TIME_HISTORY_LEN 10U
#define LAST_LOOP_TIME_MIN_VALID 100U

/* --- CAN Network Configuration --- */
#define CAN_FAST_BASE_ID 0x100U
#define CAN_SLOW_BASE_ID 0x200U
#define CAN_THERMAL_BASE_ID 0x300U
#define CAN_FAST_ID (CAN_FAST_BASE_ID + SDU_BOARD_ID)
#define CAN_SLOW_ID (CAN_SLOW_BASE_ID + SDU_BOARD_ID)
#define CAN_THERMAL_ID (CAN_THERMAL_BASE_ID + SDU_BOARD_ID)
#define CAN_RX_FILTER_ID1 0x100U
#define CAN_RX_FILTER_ID2 0x20FU

/* --- MCP3464 (ADC) & Strain Gauge Configuration --- */
#define SHOCK_POT_CHANNEL 7U
#define STRAIN_GAUGE_VALUES_PER_ROW 3U
#define MCP3464_SCAN_OSR OSR_256

/* --- Shock Potentiometer Scaling Constants --- */
#define SHOCK_POT_SLOPE 72.3327f
#define SHOCK_POT_OFFSET 0.7501f

/* --- Wheel Speed Calibration --- */
#define BOLTS_ON_HUB 6.0f
#define WHEEL_SPEED_FILTER_ALPHA 0.3f // LPF coefficient for measured RPM

/* --- Brake Thermal Calibration (MLX90614) --- */
#define BRAKE_EMISSIVITY 0.29f

/* --- Tire Thermal Calibration (MLX90640) --- */
#define MLX90640_DEFAULT_SA 0x33
#define TIRE_EMISSIVITY 0.95f
#define TA_SHIFT 8U // Reflected temp offset from ambient

#endif /* SENSOR_CALIB_CONSTANTS_H */

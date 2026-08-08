#ifndef SDU_STATE_H
#define SDU_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "sensor_calib_constants.h"

// 1. Strain Gauges
typedef struct {
    float voltages[6];                 // Processed sensor voltages in Volts
} StrainGauge_State_t;

// 2. Shock Potentiometer
typedef struct {
    float voltage;                     // Raw channel 7 voltage
    float travel_mm;                   // Converted suspension travel distance (mm)
} ShockPot_State_t;

// 3. Brake Temperature (MLX90614)
typedef struct {
    float brake_temp_c;                // Calibrated brake rotor temperature (°C)
    float ambient_temp_c;              // Ambient caliper housing temperature (°C)
    bool sensor_ready;                 // Sensor status indicator
} BrakeThermal_State_t;

// 4. Tire Temperature (MLX90640 Thermal Matrix)
typedef struct {
    float profile[32];                 // 3-row averaged center row profile (°C)
    float max_temp;                    // Hottest pixel temperature (°C)
    float min_temp;                    // Coldest pixel temperature (°C)
    float center_temp;                 // Matrix center temperature (°C)
    float ambient;                     // Sensor die temperature (°C)
    int status;                        // Sensor transaction status (0 = OK)
    volatile bool frame_ready;         // DMA scan complete signal
} TireThermal_State_t;

// 5. Wheel Speed
typedef struct {
    float input_freq;                  // Captured timer pulse edge frequency (Hz)
    float measured_rpm;                // Smoothed hub rotational velocity (RPM)
    uint32_t trigger_count;            // Accumulating capture trigger pulse counts
    uint32_t last_interrupt_timestamp; // Millisecond tick of the last trigger event
    volatile bool data_ready;          // Input capture fresh edge signal
} WheelSpeed_State_t;

// 6. Diagnostics & System Performance metrics
typedef struct {
    volatile uint32_t loop_duration_us;       // Total main execution loop time
    volatile uint32_t brake_sensor_time_us;   // Brake temp acquisition delay
    volatile uint32_t can_send_time_us;       // FDCAN payload dispatch duration
    volatile uint32_t tire_step_time_us;      // Matrix sensor DMA step transaction duration
    volatile uint32_t tire_ta_time_us;        // Thermal camera ambient parse duration
    volatile uint32_t tire_calc_time_us;      // Temperature calculation block duration
    volatile uint32_t tire_hotbar_time_us;    // Hotbar profile math math duration
    volatile uint32_t sampleCount;            // Main thread scheduler heartbeat count
    volatile uint32_t lastLoopTime[LAST_LOOP_TIME_HISTORY_LEN]; // Historical loop intervals
    volatile uint8_t lastLoopTimeIndex;       // Ring buffer pointer
} DiagnosticMetrics_t;

// Master SDU State Structure
typedef struct {
    uint32_t timestamp_ms;             // Current system tick
    StrainGauge_State_t strain_gauge;
    ShockPot_State_t shock_pot;
    BrakeThermal_State_t brake_thermal;
    TireThermal_State_t tire_thermal;
    WheelSpeed_State_t wheel_speed;
    DiagnosticMetrics_t diagnostics;
    
    // Non-blocking Network Buffer Monitoring
    struct {
        uint8_t rx_test_buffer[64];
        uint32_t rx_msg_received_count;
        uint32_t rx_last_id;
    } network;
} SDU_State_t;

#endif /* SDU_STATE_H */

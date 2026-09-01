/**
 * rate_limiting.h
 *
 *  Created on: Sept 1, 2026
 *      Author: krishaybhople
 */

#ifndef INC_RATE_LIMIT_FILTER_H_
#define INC_RATE_LIMIT_FILTER_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/**
 * @brief Rate limit filter struct.
 */
typedef struct {
    float previous_output_value;            /** Previous output filter value */
    float rise_rate_limit;                  /** Constant - rising rate limit */
    float fall_rate_limit;                  /** Constant - falling rate limit */
    uint32_t last_update_timestamp_ms;      /** Last update timestamp */
    bool initialized;                       /** To know when the MCU starts up */
} RateLimitFilter_t;

/**
 * @brief Creates a rate limit filter instance.
 * 
 * @param filter Pointer to the rate limit filter instance.
 * @param rise_rate_limit The rising rate limit.
 * @param fall_rate_limit The falling rate limit (Positive value).
 */
void RateLimitFilter_Init(RateLimitFilter_t *filter, float rise_rate_limit, float fall_rate_limit);

/**
 * @brief Update filter output using current system timestamp (HAL_GetTick).
 * @param filter Pointer to the rate limit filter instance.
 * @param input Input value.
 * @param now_ms Current system timestamp in milliseconds.
 * @return Rate-limited output value.
 */
float RateLimitFilter_Update(RateLimitFilter_t *filter, float input, uint32_t now_ms);

/**
 * @brief Reset the rate limit filter to a specified value. The next valid input
 *        becomes the initial output value.
 * @param filter Pointer to the rate limit filter instance.
 */
void RateLimitFilter_Reset(RateLimitFilter_t *filter);

#endif
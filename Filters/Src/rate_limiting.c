/**
 * rate_limiting.c
 * 
 *  Created on: Sept 1, 2026
 *      Author: krishaybhople
 */

#include "rate_limiting.h"

void RateLimitFilter_Init(RateLimitFilter_t *filter, float rise_rate_limit, float fall_rate_limit) {
    if (filter == NULL) return;

    filter->previous_output_value = 0.0f;
    filter->rise_rate_limit = fabsf(rise_rate_limit);
    filter->fall_rate_limit = fabsf(fall_rate_limit);
    filter->last_update_timestamp_ms = 0U;
    filter->initialized = false;
}

float RateLimitFilter_Update(RateLimitFilter_t *filter, float input, uint32_t now_ms) {
    if (filter == NULL) return input;
    if (isnan(input)) return filter->previous_output_value;

    if (!(filter->initialized)) {
        filter->previous_output_value = input;
        filter->last_update_timestamp_ms = now_ms;
        filter->initialized = true;
        return filter->previous_output_value;
    }

    uint32_t deltaT_ms = now_ms - filter->last_update_timestamp_ms;

    // No time elapsed after previous measurement
    if (deltaT_ms == 0U) return filter->previous_output_value;

    filter->last_update_timestamp_ms = now_ms;
    float deltaT = (float)deltaT_ms / 1000.0f;
    float deltaY = input - filter->previous_output_value;
    float max_rise = filter->rise_rate_limit*deltaT;
    float max_fall = filter->fall_rate_limit*deltaT;

    if (deltaY > max_rise)
        filter->previous_output_value += max_rise;
    else if (deltaY < -max_fall)
        filter->previous_output_value -= max_fall;
    else
        filter->previous_output_value = input;

    return filter->previous_output_value;
}

void RateLimitFilter_Reset(RateLimitFilter_t *filter) {
    if (filter == NULL) return;
    filter->initialized = false;
    filter->last_update_timestamp_ms = 0U;
}
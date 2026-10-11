/*
 * exponential_moving_average.c
 *
 *  Created on: September 8, 2026
 *      Author: KayKimKwon
 */

#include "exponential_moving_average.h"

void EMAFilter_Init(EMAFilter_t* filter, float alpha){
    if (filter == NULL) return;
    if (isnan(alpha)) return;
    if (alpha > 1.0f || alpha < 0.0f) return;

    filter->filter_alpha = alpha;
    filter->comp_alpha = (1.0f - alpha);

    filter->previous_output_value = 0.0f;
    filter->initialized = true;
}

float EMAFilter_Update(EMAFilter_t* filter, float input){
    if (filter == NULL) return input;
    if (!(filter->initialized)) return input;
    if (isnan(input)) return filter->previous_output_value;

    // room for optimization
    float filtered_input = filter->filter_alpha * input;
    filter->previous_output_value *= filter->comp_alpha;
    filter->previous_output_value += filtered_input;

    return filter->previous_output_value;
    
}

void EMAFilter_Reset(EMAFilter_t* filter){
    if (filter == NULL) return;

    // 100% smoothing factor, the filter does not depend on past historical data inputs at all
    filter->filter_alpha = 1.0f; 
    filter->comp_alpha = 0.0f;

    filter->previous_output_value = 0.0f;
    filter->initialized = false;
}
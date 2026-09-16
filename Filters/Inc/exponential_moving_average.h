/*
 * exponential_moving_average.h
 *
 *  Created on: September 8, 2026
 *      Author: KayKimKwon
 */

#ifndef INC_EXPONENTIAL_MOVING_AVERAGE_FILTER_H_
#define INC_EXPONENTIAL_MOVING_AVERAGE_FILTER_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <math.h>

/**
 * @brief EMA filter struct.
 */
typedef struct{
    float filter_alpha;                 /** Smoothing factor - range between 0 and 1, weight given to new data */
    float comp_alpha;                   /** Inertia factor - weight given to historical data */
    float previous_output_value;        /** Previous output filter value */
    bool initialized;                   /** True after alpha value is given */
} EMAFilter_t;

/**
 * @brief Creates a EMA filter instance.
 * 
 * @param filter Pointer to the EMA filter instance.
 * @param alpha The smoothing factor
 */
void EMAFilter_Init(EMAFilter_t* filter, float alpha);


/**
 * @brief Update filter output with new given input
 * 
 * @param filter Pointer to the EMA filter instance.
 * @param input New data input value
 * @return most recent valid EMA output value.
 */
float EMAFilter_Update(EMAFilter_t* filter, float input);


/**
 * @brief Reset the EMA filter to 100% smoothing factor and its output. Needs to be initialized with a new alpha value before the next valid input
 *        is given.
 * 
 * @param filter Pointer to the EMA filter instance.
 */
void EMAFilter_Reset(EMAFilter_t* filter);


#endif /* INC_EXPONENTIAL_MOVING_AVERAGE_FILTER_H_ */
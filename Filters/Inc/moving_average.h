/*
 * moving_average.h
 *
 *  Created on: Aug 5, 2026
 *      Author: krishaybhople
 */

#ifndef FILTERS_MOVING_AVERAGE_H_
#define FILTERS_MOVING_AVERAGE_H_

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Moving average filter instance structure.
 */
typedef struct {
    float *buffer;        /** Pointer to the data buffer */
    uint32_t size;        /** Size of the filter buffer */
    uint32_t index;       /** Current write index in the buffer */
    float sum;            /** Cumulative sum of elements in the buffer (guarded against drift) */
    float avg;            /** Current moving average value */
    uint8_t initialized;  /** Flag indicating if the buffer has been filled initially */
} MovingAverage_t;

/**
 * @brief Initializes the moving average filter.
 * 
 * @param filter Pointer to the MovingAverage_t instance.
 * @param buffer Pointer to a pre-allocated float buffer of size elements.
 * @param size Number of elements in the filter window.
 */
void MovingAverage_Init(MovingAverage_t *filter, float *buffer, uint32_t size);

/**
 * @brief Updates the filter with a new sample and returns the new average.
 * 
 * @param filter Pointer to the MovingAverage_t instance.
 * @param sample The new data sample to add to the filter.
 * @return The updated moving average value.
 */
float MovingAverage_Update(MovingAverage_t *filter, float sample);

/**
 * @brief Resets the filter state and clear history.
 * 
 * @param filter Pointer to the MovingAverage_t instance.
 */
void MovingAverage_Reset(MovingAverage_t *filter);

#endif /* FILTERS_MOVING_AVERAGE_H_ */

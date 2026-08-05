/*
 * moving_average.c
 *
 *  Created on: Aug 5, 2026
 *      Author: krishaybhople
 */

#include "moving_average.h"

void MovingAverage_Init(MovingAverage_t *filter, float *buffer, uint32_t size)
{
    if (filter == NULL || buffer == NULL || size == 0)
    {
        return;
    }

    filter->buffer = buffer;
    filter->size = size;
    MovingAverage_Reset(filter);
}

float MovingAverage_Update(MovingAverage_t *filter, float sample)
{
    if (filter == NULL || filter->buffer == NULL || filter->size == 0)
    {
        return 0.0f;
    }

    if (!filter->initialized)
    {
        filter->sum = 0.0f;
        for (uint32_t i = 0; i < filter->size; i++)
        {
            filter->buffer[i] = sample;
            filter->sum += sample;
        }
        filter->avg = sample;
        filter->index = 0;
        filter->initialized = 1;
        return filter->avg;
    }

    // Subtract the oldest sample from sum
    filter->sum -= filter->buffer[filter->index];

    // Store new sample
    filter->buffer[filter->index] = sample;

    // Add new sample to sum
    filter->sum += sample;

    // Advance write pointer
    filter->index++;
    if (filter->index >= filter->size)
    {
        filter->index = 0;

        // Periodically recalculate the exact sum to prevent accumulated floating-point
        // round-off/precision drift over long execution periods.
        float exact_sum = 0.0f;
        for (uint32_t i = 0; i < filter->size; i++)
        {
            exact_sum += filter->buffer[i];
        }
        filter->sum = exact_sum;
    }

    // Calculate average
    filter->avg = filter->sum / (float)filter->size;

    return filter->avg;
}

void MovingAverage_Reset(MovingAverage_t *filter)
{
    if (filter == NULL)
    {
        return;
    }

    filter->index = 0;
    filter->sum = 0.0f;
    filter->avg = 0.0f;
    filter->initialized = 0;

    if (filter->buffer != NULL)
    {
        for (uint32_t i = 0; i < filter->size; i++)
        {
            filter->buffer[i] = 0.0f;
        }
    }
}

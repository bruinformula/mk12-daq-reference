/*
 * savitzky_golay.h
 *
 *  Created on: Sep 3, 2026
 *      Author: tuhinmondal
 */

 #include "savitzky_golay.h"


void SavitzkyGolay_Init(SavitzkyGolayFilter_t *filter, float *buffer, uint16_t window_size, const float *coefficients, float coefficient_sum)
{
    // Store the filter configuration.
    filter->buffer = buffer;
    filter->window_size = window_size;
    filter->coefficients = coefficients;
    filter->coefficient_sum = coefficient_sum;

    // Initialize the circular buffer state.
    filter->index = 0;
    filter->samples_filled = 0;

    /*
     * Clear the sample buffer.
     *
     * This prevents uninitialized memory from being used
     * during filter startup.
     */
    for (uint16_t i = 0; i < window_size; i++)
    {
        filter->buffer[i] = 0.0f;
    }
}


float SavitzkyGolay_Update(SavitzkyGolayFilter_t *filter, float input)
{
    // Store the new sample at the current write index.
    filter->buffer[filter->index] = input;

    // Move to the next position in the circular buffer.
    filter->index++;

    // Wrap around when the end of the buffer is reached.
    if (filter->index >= filter->window_size)
    {
        filter->index = 0;
    }

    // Keep track of how many valid samples are available.
    if (filter->samples_filled < filter->window_size)
    {
        filter->samples_filled++;
    }

    /*
     * During startup, there aren't enough samples to
     * perform the complete Savitzky-Golay calculation.
     *
     * Returning the raw input avoids producing an
     * artificial transient caused by the initially
     * empty buffer.
     */
    if (filter->samples_filled < filter->window_size)
    {
        return input;
    }

    // Calculate the weighted sum of the samples.
    float output = 0.0f;

    for (uint16_t i = 0; i < filter->window_size; i++)
    {
        /*
         * Start at the oldest sample and move forward
         * through the circular buffer.
         */
        uint16_t buffer_index = filter->index + i;

        // Wrap around if necessary.
        if (buffer_index >= filter->window_size)
        {
            buffer_index -= filter->window_size;
        }

        // Multiply the sample by its corresponding Savitzky-Golay coefficient.
        output += filter->buffer[buffer_index]
                * filter->coefficients[i];
    }

    // Normalize the result by the sum of the filter coefficients.
    output /= filter->coefficient_sum;

    return output;
}
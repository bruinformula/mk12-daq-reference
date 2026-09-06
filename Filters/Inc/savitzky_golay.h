/*
 * savitzky_golay.h
 *
 *  Created on: Sep 3, 2026
 *      Author: tuhinmondal
 */

#ifndef INC_SAVITZKY_GOLAY_FILTER_H_
#define INC_SAVITZKY_GOLAY_FILTER_H_

#include <stdint.h>

/**
 * @brief Savitzky-Golay filter instance structure.
 */
typedef struct
{
    float *buffer;              /** Pointer to the circular buffer storing the most recent samples. */
    uint16_t window_size;       /** Number of samples in the filter window. Must be an odd number. */
    uint16_t index;             /** Current index in the circular buffer where the next sample will be stored. */
    uint16_t samples_filled;    /** Number of valid samples currently stored in the buffer. */
    
    const float *coefficients;  /** Pointer to the precomputed Savitzky-Golay filter coefficients. */
    float coefficient_sum;      /** Sum of the filter coefficients, used to normalize the output. */

} SavitzkyGolayFilter_t;

/**
 * @brief Initialize a Savitzky-Golay filter.
 *
 * @param filter Pointer to the SavitzkyGolayFilter_t instance to initialize.
 * @param buffer Pointer to the memory buffer used to store input samples.
 * @param window_size Number of samples in the Savitzky-Golay filter window.
 * @param coefficients Pointer to the precomputed filter coefficients.
 * @param coefficient_sum Sum of the filter coefficients used for normalization.
 *
 * @note The window size should be an odd number and greater than the
 *       polynomial order used to generate the coefficients.
 */
void SavitzkyGolay_Init(SavitzkyGolayFilter_t *filter, float *buffer, uint16_t window_size, const float *coefficients, float coefficient_sum);

/**
 * @brief Add a new sample and calculate the filtered output.
 *
 * @param filter Pointer to the SavitzkyGolayFilter_t instance.
 * @param input New unfiltered sensor sample.
 *
 * @return The Savitzky-Golay filtered value. During initialization, the
 *         input value is returned until the filter buffer is filled.
 */
float SavitzkyGolay_Update(SavitzkyGolayFilter_t *filter, float input);

#endif

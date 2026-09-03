/**
 * kalman_filter.c
 *
 *  Created on: Sept 3, 2026
 *      Author: RonitBarman
 */

#include "kalman_filter.h"

#include <float.h>
#include <string.h>
#include <math.h>

//Threshold used to reject a singular residual covariance matrix.
#define KALMAN_FILTER_INVERSION_EPSILON (10.0f * FLT_EPSILON)


// (variable)^- means previous value

//Inverts an active measurement-size matrix with pivoted Gauss-Jordan elimination. 
static bool KalmanFilter_InvertMeasurementMatrix(
    float matrix[KALMAN_FILTER_MAX_MEASUREMENTS][KALMAN_FILTER_MAX_MEASUREMENTS],
    float inverse[KALMAN_FILTER_MAX_MEASUREMENTS][KALMAN_FILTER_MAX_MEASUREMENTS],
    uint8_t dimension)
{
    uint8_t row;
    uint8_t column;
    uint8_t pivot_row;

    // row operations turn I (identity matrix) into matrix^-1. 
    for (row = 0U; row < dimension; row++) {
        for (column = 0U; column < dimension; column++) {
            inverse[row][column] = (row == column) ? 1.0f : 0.0f;
        }
    }

    for (column = 0U; column < dimension; column++) {
        float largest_pivot = fabsf(matrix[column][column]);
        pivot_row = column;
        for (row = (uint8_t)(column + 1U); row < dimension; row++) {
            const float candidate = fabsf(matrix[row][column]);
            if (candidate > largest_pivot) {
                largest_pivot = candidate;
                pivot_row = row;
            }
        }
        if (largest_pivot <= KALMAN_FILTER_INVERSION_EPSILON) {
            return false;
        }
        if (pivot_row != column) {
            for (uint8_t index = 0U; index < dimension; index++) {
                float temporary = matrix[column][index];
                matrix[column][index] = matrix[pivot_row][index];
                matrix[pivot_row][index] = temporary;
                temporary = inverse[column][index];
                inverse[column][index] = inverse[pivot_row][index];
                inverse[pivot_row][index] = temporary;
            }
        }
        const float pivot_inverse = 1.0f / matrix[column][column];
        for (uint8_t index = 0U; index < dimension; index++) {
            matrix[column][index] *= pivot_inverse;
            inverse[column][index] *= pivot_inverse;
        }
        for (row = 0U; row < dimension; row++) {
            if (row != column) {
                const float factor = matrix[row][column];
                for (uint8_t index = 0U; index < dimension; index++) {
                    matrix[row][index] -= factor * matrix[column][index];
                    inverse[row][index] -= factor * inverse[column][index];
                }
            }
        }
    }
    return true;
}

bool KalmanFilter_Init(KalmanFilter_t *filter, uint8_t state_dimension, uint8_t measurement_dimension, uint8_t input_dimension)
{
    (void)memset(filter, 0, sizeof(*filter));
    filter->state_dimension = state_dimension;
    filter->measurement_dimension = measurement_dimension;
    filter->input_dimension = input_dimension;

    // Set A, P and R to identity matrices by default
    for (uint8_t index = 0U; index < state_dimension; index++) {
        filter->A[index][index] = 1.0f;
        filter->P[index][index] = 1.0f;
    }

    for (uint8_t index = 0U; index < measurement_dimension; index++) {
        filter->R[index][index] = 1.0f;
    }
    return true;
}

bool KalmanFilter_Predict(KalmanFilter_t *filter, const float *input)
{
    const uint8_t n = filter->state_dimension;
    const uint8_t input_count = filter->input_dimension;

    // Predict state: x^- = A*x + B*u.
    for (uint8_t row = 0U; row < n; row++) {
        float predicted_state = 0.0f;
        for (uint8_t column = 0U; column < n; column++) {
            predicted_state += filter->A[row][column] * filter->x[column];
        }
        for (uint8_t column = 0U; column < input_count; column++) {
            predicted_state += filter->B[row][column] * input[column];
        }
        filter->workspace_nn_1[row][0] = predicted_state;
    }

    // Store x^- and calculate the first covariance product A*P
    for (uint8_t row = 0U; row < n; row++) {
        filter->x[row] = filter->workspace_nn_1[row][0];
        for (uint8_t column = 0U; column < n; column++) {
            float value = 0.0f;
            for (uint8_t index = 0U; index < n; index++) {
                value += filter->A[row][index] * filter->P[index][column];
            }
            filter->workspace_nn_1[row][column] = value;
        }
    }

    // Predict covariance: P^- = (A*P)*A^T + Q
    for (uint8_t row = 0U; row < n; row++) {
        for (uint8_t column = 0U; column < n; column++) {
            float value = filter->Q[row][column];
            for (uint8_t index = 0U; index < n; index++) {
                value += filter->workspace_nn_1[row][index] * filter->A[column][index];
            }
            filter->workspace_nn_2[row][column] = value;
        }
    }

    // Store the predicted covariance P^-
    for (uint8_t row = 0U; row < n; row++) {
        for (uint8_t column = 0U; column < n; column++) {
            filter->P[row][column] = filter->workspace_nn_2[row][column];
        }
    }
    return true;
}

bool KalmanFilter_Update(KalmanFilter_t *filter, const float *measurement)
{
    const uint8_t n = filter->state_dimension;
    const uint8_t m = filter->measurement_dimension;

    // Measurement residual: y = z - H*x^-
    for (uint8_t row = 0U; row < m; row++) {
        float predicted_measurement = 0.0f;
        for (uint8_t index = 0U; index < n; index++) {
            predicted_measurement += filter->H[row][index] * filter->x[index];
        }
        filter->residual[row] = measurement[row] - predicted_measurement;
    }

    // Shared gain term: P^-*H^T
    for (uint8_t row = 0U; row < n; row++) {
        for (uint8_t column = 0U; column < m; column++) {
            float value = 0.0f;
            for (uint8_t index = 0U; index < n; index++) {
                value += filter->P[row][index] * filter->H[column][index];
            }
            filter->workspace_nm[row][column] = value;
        }
    }

    // Innovation covariance: S = H*P^-*H^T + R
    for (uint8_t row = 0U; row < m; row++) {
        for (uint8_t column = 0U; column < m; column++) {
            float value = filter->R[row][column];
            for (uint8_t index = 0U; index < n; index++) {
                value += filter->H[row][index] * filter->workspace_nm[index][column];
            }
            filter->workspace_mm_1[row][column] = value;
        }
    }

    // Invert S so measurement uncertainty can be used in the gain
    if (!KalmanFilter_InvertMeasurementMatrix(filter->workspace_mm_1, filter->workspace_mm_2, m)) {
        return false;
    }

    // Kalman gain: K = P^-*H^T*S^-1
    for (uint8_t row = 0U; row < n; row++) {
        for (uint8_t column = 0U; column < m; column++) {
            float value = 0.0f;
            for (uint8_t index = 0U; index < m; index++) {
                value += filter->workspace_nm[row][index] * filter->workspace_mm_2[index][column];
            }
            filter->K[row][column] = value;
        }
    }

    // Form I - K*H for the Joseph covariance update
    for (uint8_t row = 0U; row < n; row++) {
        for (uint8_t column = 0U; column < n; column++) {
            float value = (row == column) ? 1.0f : 0.0f;
            for (uint8_t index = 0U; index < m; index++) {
                value -= filter->K[row][index] * filter->H[index][column];
            }
            filter->workspace_nn_1[row][column] = value;
        }
    }

    // Correct state to x = x^- + K*y and calculate (I - K*H)*P^-
    for (uint8_t row = 0U; row < n; row++) {
        float correction = 0.0f;
        for (uint8_t index = 0U; index < m; index++) {
            correction += filter->K[row][index] * filter->residual[index];
        }
        filter->x[row] += correction;
        for (uint8_t column = 0U; column < n; column++) {
            float value = 0.0f;
            for (uint8_t index = 0U; index < n; index++) {
                value += filter->workspace_nn_1[row][index] * filter->P[index][column];
            }
            filter->workspace_nn_2[row][column] = value;
        }
    }

    // First Joseph term: P = (I - K*H)*P^-*(I - K*H)^T
    for (uint8_t row = 0U; row < n; row++) {
        for (uint8_t column = 0U; column < n; column++) {
            float value = 0.0f;
            for (uint8_t index = 0U; index < n; index++) {
                value += filter->workspace_nn_2[row][index] * filter->workspace_nn_1[column][index];
            }
            filter->P[row][column] = value;
        }
    }

    // Complete Joseph form: P += K*R*K^T.
    for (uint8_t row = 0U; row < n; row++) {
        for (uint8_t column = 0U; column < n; column++) {
            float noise_term = 0.0f;
            for (uint8_t measurement_row = 0U; measurement_row < m; measurement_row++) {
                for (uint8_t measurement_column = 0U; measurement_column < m;
                     measurement_column++) {
                    noise_term += filter->K[row][measurement_row] *
                                  filter->R[measurement_row][measurement_column] *
                                  filter->K[column][measurement_column];
                }
            }
            filter->P[row][column] += noise_term;
        }
    }
    return true;
}

void KalmanFilter_Reset(KalmanFilter_t *filter, const float *initial_state, const float *initial_covariance)
{
    const uint8_t n = filter->state_dimension;
    (void)memset(filter->x, 0, sizeof(filter->x));
    (void)memset(filter->P, 0, sizeof(filter->P));
    (void)memset(filter->K, 0, sizeof(filter->K));
    (void)memset(filter->residual, 0, sizeof(filter->residual));

    // Restore x and P from caller values, or use x = 0 and P = I
    for (uint8_t row = 0U; row < n; row++) {
        if (initial_state != NULL) {
            filter->x[row] = initial_state[row];
        }
        for (uint8_t column = 0U; column < n; column++) {
            if (initial_covariance != NULL) {
                filter->P[row][column] = initial_covariance[((size_t)row * n) + column];
            } else if (row == column) {
                filter->P[row][column] = 1.0f;
            }
        }
    }
}

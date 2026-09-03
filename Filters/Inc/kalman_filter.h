/**
 * kalman_filter.h
 * 
 * Created on: Sep 3, 2026
 *      Author: RonitBarman
 */


#ifndef INC_KALMAN_FILTER_H_
#define INC_KALMAN_FILTER_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/** Maximum number of states supported by one filter instance. */
#ifndef KALMAN_FILTER_MAX_STATES
#define KALMAN_FILTER_MAX_STATES       6U
#endif

/** Maximum number of measurements accepted in one update. */
#ifndef KALMAN_FILTER_MAX_MEASUREMENTS
#define KALMAN_FILTER_MAX_MEASUREMENTS 3U
#endif

/** Maximum number of control inputs accepted in one prediction. */
#ifndef KALMAN_FILTER_MAX_INPUTS
#define KALMAN_FILTER_MAX_INPUTS       3U
#endif

/**
 * @brief Fixed-memory linear Kalman filter instance.
 * @note Matrices are row-major: matrix[row][column]. Configure A, B, H, Q and R
 *       after initialization. State estimate x and covariance P are public for
 *       simple embedded inspection and logging.
 */
typedef struct {
    uint8_t state_dimension;                                      /** Number of active states */
    uint8_t measurement_dimension;                                /** Number of active measurements */
    uint8_t input_dimension;                                      /** Number of active control inputs */
    bool initialized;                                             /** True after successful initialization */
    float x[KALMAN_FILTER_MAX_STATES];                             /** State estimate,  */
    float P[KALMAN_FILTER_MAX_STATES][KALMAN_FILTER_MAX_STATES];  /** State covariance */
    float A[KALMAN_FILTER_MAX_STATES][KALMAN_FILTER_MAX_STATES];  /** State transition matrix */
    float B[KALMAN_FILTER_MAX_STATES][KALMAN_FILTER_MAX_INPUTS];  /** Control input matrix */
    float Q[KALMAN_FILTER_MAX_STATES][KALMAN_FILTER_MAX_STATES];  /** Process noise covariance */
    float H[KALMAN_FILTER_MAX_MEASUREMENTS][KALMAN_FILTER_MAX_STATES]; /** Measurement matrix */
    float R[KALMAN_FILTER_MAX_MEASUREMENTS][KALMAN_FILTER_MAX_MEASUREMENTS]; /** Measurement noise covariance */
    float K[KALMAN_FILTER_MAX_STATES][KALMAN_FILTER_MAX_MEASUREMENTS]; /** Last Kalman gain */
    float workspace_nn_1[KALMAN_FILTER_MAX_STATES][KALMAN_FILTER_MAX_STATES];
    float workspace_nn_2[KALMAN_FILTER_MAX_STATES][KALMAN_FILTER_MAX_STATES];
    float workspace_nm[KALMAN_FILTER_MAX_STATES][KALMAN_FILTER_MAX_MEASUREMENTS];
    float workspace_mm_1[KALMAN_FILTER_MAX_MEASUREMENTS][KALMAN_FILTER_MAX_MEASUREMENTS];
    float workspace_mm_2[KALMAN_FILTER_MAX_MEASUREMENTS][KALMAN_FILTER_MAX_MEASUREMENTS];
    float residual[KALMAN_FILTER_MAX_MEASUREMENTS];
} KalmanFilter_t;

/**
 * @brief Initializes a linear Kalman filter instance.
 * @param filter Pointer to the filter instance.
 * @param state_dimension Number of active states.
 * @param measurement_dimension Number of measurements per update.
 * @param input_dimension Number of control inputs; zero is allowed.
 * @return True when the dimensions are valid and initialization succeeds.
 * @note A, P and R start as identity matrices. B, H, Q, x and K start at zero.
 */
bool KalmanFilter_Init(KalmanFilter_t *filter, uint8_t state_dimension,
                       uint8_t measurement_dimension, uint8_t input_dimension);

/**
 * @brief Predicts the state and covariance using x = A*x + B*u.
 * @param filter Pointer to the filter instance.
 * @param input Control input vector, or NULL when input_dimension is zero.
 * @return True on success; false for an invalid filter or missing input.
 */
bool KalmanFilter_Predict(KalmanFilter_t *filter, const float *input);

/**
 * @brief Corrects the state and covariance using a measurement vector.
 * @param filter Pointer to the filter instance.
 * @param measurement Measurement vector with measurement_dimension elements.
 * @return True on success; false if an argument is invalid or the innovation
 *         covariance cannot be inverted.
 */
bool KalmanFilter_Update(KalmanFilter_t *filter, const float *measurement);

/**
 * @brief Resets the estimate while preserving A, B, H, Q and R.
 * @param filter Pointer to the filter instance.
 * @param initial_state Initial state vector, or NULL for a zero state.
 * @param initial_covariance Initial row-major covariance matrix, or NULL for
 *        identity covariance.
 */
void KalmanFilter_Reset(KalmanFilter_t *filter, const float *initial_state,
                        const float *initial_covariance);

#endif /* INC_KALMAN_FILTER_H_ */
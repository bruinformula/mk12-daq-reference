#ifndef LSM6DSO_H
#define LSM6DSO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

#define IMU_WHO_AM_I_VALUE  0x6C

HAL_StatusTypeDef IMU_Init(SPI_HandleTypeDef *spi, GPIO_TypeDef *cs_port, uint16_t cs_pin);
HAL_StatusTypeDef IMU_CheckWhoAmI(uint8_t *whoami);
HAL_StatusTypeDef IMU_ReadAxes(int16_t *gx, int16_t *gy, int16_t *gz,
		int16_t *ax, int16_t *ay, int16_t *az);

HAL_StatusTypeDef IMU_StartReadAxes(void);
uint8_t IMU_PollReadAxes(int16_t *gx, int16_t *gy, int16_t *gz,
		int16_t *ax, int16_t *ay, int16_t *az);
uint8_t IMU_IsBusy(void);

#ifdef __cplusplus
}
#endif

#endif /* LSM6DSO_H */

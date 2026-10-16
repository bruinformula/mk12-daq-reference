/**
 * @file    fdcan.h
 * @brief   Bosch M_CAN / FDCAN1 Driver for STM32U5 (PB8 RX, PB9 TX)
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#ifndef INC_FDCAN_H_
#define INC_FDCAN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef struct {
    uint32_t id;
    uint8_t  data[64];
    uint8_t  len;
    bool     is_fd;
    bool     brs;
} FDCAN_Message_t;

/**
 * @brief Initialize FDCAN1 hardware peripheral and PB8/PB9 GPIO pins.
 */
bool FDCAN_Driver_Init(void);

/**
 * @brief Start FDCAN1 controller.
 */
bool FDCAN_Driver_Start(void);

/**
 * @brief Stop FDCAN1 controller and reset to initial state.
 */
void FDCAN_Driver_Stop(void);

/**
 * @brief Poll RX FIFO 0 for an incoming frame.
 * @return true if a message was received, false if FIFO empty.
 */
bool FDCAN_Driver_Receive(FDCAN_Message_t *msg);

/**
 * @brief Transmit a frame over FDCAN1.
 * @return true if message added to TX FIFO successfully.
 */
bool FDCAN_Driver_Transmit(const FDCAN_Message_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* INC_FDCAN_H_ */

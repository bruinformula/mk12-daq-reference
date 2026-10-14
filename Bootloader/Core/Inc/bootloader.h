/**
 * @file    bootloader.h
 * @brief   BFR CAN Bootloader Protocol Engine & State Machine
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#ifndef INC_BOOTLOADER_H_
#define INC_BOOTLOADER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ========================================================================= */
/* Command Opcodes & Status Codes                                            */
/* ========================================================================= */

#define CMD_ENTER_BOOTLOADER        0x01U
#define CMD_PING                    0x02U
#define CMD_ERASE_APP               0x03U
#define CMD_WRITE_DATA              0x04U
#define CMD_VERIFY                  0x05U
#define CMD_ACK                     0x06U
#define CMD_JUMP_APP                0x07U
#define CMD_MODE_NEGOTIATE          0x08U
#define CMD_STREAM_HEADER           0x09U
#define CMD_STREAM_DATA             0x0AU
#define CMD_NACK                    0x15U

#define BOOT_MODE_BOOTLOADER        0x01U
#define BOOT_MODE_APP               0x02U

#define BOOT_STATUS_OK              0x00U
#define BOOT_STATUS_ERROR           0x01U
#define BOOT_STATUS_CRC_FAIL        0x02U
#define BOOT_STATUS_INVALID_ADDR    0x03U
#define BOOT_STATUS_BUSY            0x04U

/* Block sizes for streaming writes */
#define CLASSIC_CAN_BLOCK_SIZE      96U   /* 12 frames * 8 bytes = 96 bytes (6 quad-words) */
#define FDCAN_FRAME_PAYLOAD_SIZE    48U   /* 48 bytes (3 quad-words) per FD frame */
#define QUADWORD_SIZE               16U   /* 128-bit flash write unit */

/* ========================================================================= */
/* Function Prototypes                                                       */
/* ========================================================================= */

/**
 * @brief Initialize bootloader peripherals and state.
 */
void Bootloader_Init(void);

/**
 * @brief Main execution loop of the bootloader.
 */
void Bootloader_Run(void);

/**
 * @brief Check if a valid application image exists at APPLICATION_START_ADDR.
 * @return true if initial stack pointer and reset handler appear valid.
 */
bool Bootloader_CheckAppValid(void);

/**
 * @brief Perform clean peripheral shutdown and branch to the application.
 */
void Bootloader_JumpToApp(void);

/**
 * @brief Process an incoming CAN frame and generate a response if applicable.
 * @param rx_id   CAN identifier received
 * @param rx_data Payload received
 * @param rx_len  Length of payload received
 * @param tx_data Output buffer for reply
 * @param tx_len  Output pointer for reply length
 * @param tx_id   Output pointer for reply CAN ID
 * @return true if a response frame should be sent back to host
 */
bool Bootloader_ProcessMessage(uint32_t rx_id, const uint8_t *rx_data, uint8_t rx_len,
                              uint8_t *tx_data, uint8_t *tx_len, uint32_t *tx_id);

#ifdef __cplusplus
}
#endif

#endif /* INC_BOOTLOADER_H_ */

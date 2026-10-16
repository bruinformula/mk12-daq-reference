/**
 * @file    bootloader_client.h
 * @brief   BFR DAQ In-Application CAN Bootloader Hook
 * @details Integrates into board application firmware (running at 0x08008000)
 *          to listen for host CAN commands (bfr can-flash / bfr can-scan),
 *          set the bootloader magic key, and perform an NVIC system reset.
 *
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#ifndef INC_BOOTLOADER_CLIENT_H_
#define INC_BOOTLOADER_CLIENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* ========================================================================= */
/* Bootloader Protocol Definitions                                           */
/* ========================================================================= */

#define BOOTLOADER_MAGIC_KEY            0xDEADBEEFU
#define BOOTLOADER_MAGIC_ADDR           0x20000000U /* Base of SRAM */

/* Standard Broadcast CAN ID */
#define CAN_ID_BOOT_BROADCAST           0x07FU

/* Command Opcodes */
#define CMD_ENTER_BOOTLOADER            0x01U
#define CMD_PING                        0x02U
#define CMD_ERASE_APP                   0x03U
#define CMD_WRITE_DATA                  0x04U
#define CMD_VERIFY                      0x05U
#define CMD_ACK                         0x06U
#define CMD_NACK                        0x15U
#define CMD_JUMP_APP                    0x07U
#define CMD_MODE_NEGOTIATE              0x08U

/* Operating Modes reported on PING */
#define BOOT_MODE_BOOTLOADER            0x01U
#define BOOT_MODE_APP                   0x02U

/* Response Status Codes */
#define BOOT_STATUS_OK                  0x00U
#define BOOT_STATUS_ERROR               0x01U
#define BOOT_STATUS_CRC_FAIL            0x02U
#define BOOT_STATUS_INVALID_ADDR        0x03U

/* Firmware Version (Major.Minor.Patch) */
#define APP_FW_VERSION_MAJOR            1U
#define APP_FW_VERSION_MINOR            0U
#define APP_FW_VERSION_PATCH            0U

/* ========================================================================= */
/* Board CAN ID Mapping (Mk. 12 DAQ Bus)                                    */
/* ========================================================================= */

typedef enum {
    BOARD_ID_MDU   = 0x01,  /* Master Data Unit */
    BOARD_ID_DVCU  = 0x02,  /* DAQ Vehicle Control Unit */
    BOARD_ID_SDU   = 0x03,  /* Suspension Data Unit */
    BOARD_ID_SMU   = 0x04,  /* Spatial Monitoring Unit */
    BOARD_ID_SKU   = 0x05,  /* Suspension & Kinematics Unit */
    BOARD_ID_PMU   = 0x06,  /* Pneumatic Measurement Unit */
    BOARD_ID_TSMU  = 0x07,  /* Tractive System Measurement Unit */
    BOARD_ID_SGU   = 0x08,  /* Strain Gauge Unit */
    BOARD_ID_ALL   = 0xFF   /* Global Broadcast */
} BFR_BoardID_t;

/* Standard Target (Host -> Board) & Response (Board -> Host) CAN IDs */
#define CAN_ID_TARGET_MDU               0x010U
#define CAN_ID_RESP_MDU                 0x011U

#define CAN_ID_TARGET_DVCU              0x012U
#define CAN_ID_RESP_DVCU                0x013U

#define CAN_ID_TARGET_SDU               0x014U
#define CAN_ID_RESP_SDU                 0x015U

#define CAN_ID_TARGET_SMU               0x016U
#define CAN_ID_RESP_SMU                 0x017U

#define CAN_ID_TARGET_SKU               0x018U
#define CAN_ID_RESP_SKU                 0x019U

#define CAN_ID_TARGET_PMU               0x01AU
#define CAN_ID_RESP_PMU                 0x01BU

#define CAN_ID_TARGET_TSMU              0x01CU
#define CAN_ID_RESP_TSMU                0x01DU

#define CAN_ID_TARGET_SGU               0x01EU
#define CAN_ID_RESP_SGU                 0x01FU

/* ========================================================================= */
/* Board CAN ID Mapping (Mk. 11 DAQ Bus)                                    */
/* ========================================================================= */

#define BOARD_ID_MK11_MDU               0x11
#define BOARD_ID_MK11_TSPMU             0x12
#define BOARD_ID_MK11_SDU               0x13
#define BOARD_ID_MK11_SMU               0x14
#define BOARD_ID_MK11_TSHMU             0x15

#define CAN_ID_TARGET_MK11_MDU          0x020U
#define CAN_ID_RESP_MK11_MDU            0x021U

#define CAN_ID_TARGET_MK11_TSPMU        0x022U
#define CAN_ID_RESP_MK11_TSPMU          0x023U

#define CAN_ID_TARGET_MK11_SDU          0x024U
#define CAN_ID_RESP_MK11_SDU            0x025U

#define CAN_ID_TARGET_MK11_SMU          0x026U
#define CAN_ID_RESP_MK11_SMU            0x027U

#define CAN_ID_TARGET_MK11_TSHMU        0x028U
#define CAN_ID_RESP_MK11_TSHMU          0x029U

/* ========================================================================= */
/* Function Prototypes                                                       */
/* ========================================================================= */

/**
 * @brief Initialize the in-application bootloader client.
 * @param board_id        One of BFR_BoardID_t enum
 * @param target_can_id   CAN ID the host uses to command this board (e.g. CAN_ID_TARGET_SDU)
 * @param response_can_id CAN ID this board uses to reply to the host (e.g. CAN_ID_RESP_SDU)
 */
void BootloaderClient_Init(uint8_t board_id, uint32_t target_can_id, uint32_t response_can_id);

/**
 * @brief Core frame inspection logic independent of HAL type.
 * @param rx_id       Received standard CAN ID
 * @param rx_data     Pointer to received payload
 * @param rx_len      Payload length (DLC)
 * @param reply_data  Buffer to hold reply data (at least 8 bytes)
 * @param reply_len   Pointer to store length of reply payload
 * @param reply_id    Pointer to store CAN ID to send reply on
 * @param trigger_reboot Set to true if the system must reboot into bootloader
 * @return true if message was processed by bootloader client, false if ignored
 */
bool BootloaderClient_HandleFrame(uint32_t rx_id, const uint8_t *rx_data, uint8_t rx_len,
                                 uint8_t *reply_data, uint8_t *reply_len, uint32_t *reply_id,
                                 bool *trigger_reboot);

/**
 * @brief Arm the bootloader magic key in RAM and trigger MCU reset.
 */
void BootloaderClient_EnterBootloader(void);

#if defined(HAL_FDCAN_MODULE_ENABLED)
/**
 * @brief Convenience hook for STM32 FDCAN (STM32U5, H7, G4).
 *        Call this inside HAL_FDCAN_RxFifo0Callback().
 * @param hfdcan    Pointer to FDCAN handle
 * @param rx_header Pointer to received FDCAN header
 * @param rx_data   Pointer to received data payload
 * @return true if handled by bootloader client, false otherwise
 */
bool BootloaderClient_ProcessFDCAN(FDCAN_HandleTypeDef *hfdcan,
                                   FDCAN_RxHeaderTypeDef *rx_header,
                                   const uint8_t *rx_data);
#endif

#if defined(HAL_CAN_MODULE_ENABLED)
/**
 * @brief Convenience hook for STM32 Classic bxCAN (STM32F4, F1, L4).
 *        Call this inside HAL_CAN_RxFifo0MsgPendingCallback().
 * @param hcan      Pointer to CAN handle
 * @param rx_header Pointer to received CAN header
 * @param rx_data   Pointer to received data payload
 * @return true if handled by bootloader client, false otherwise
 */
bool BootloaderClient_ProcessClassicCAN(CAN_HandleTypeDef *hcan,
                                        CAN_RxHeaderTypeDef *rx_header,
                                        const uint8_t *rx_data);
#endif

#ifdef __cplusplus
}
#endif

#endif /* INC_BOOTLOADER_CLIENT_H_ */

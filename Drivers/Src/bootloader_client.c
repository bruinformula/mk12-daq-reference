/**
 * @file    bootloader_client.c
 * @brief   BFR DAQ In-Application CAN Bootloader Hook Implementation
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#include "bootloader_client.h"
#include <string.h>

static uint8_t  s_board_id        = 0;
static uint32_t s_target_can_id    = 0;
static uint32_t s_response_can_id  = 0;
static bool     s_initialized      = false;

void BootloaderClient_Init(uint8_t board_id, uint32_t target_can_id, uint32_t response_can_id)
{
    s_board_id        = board_id;
    s_target_can_id    = target_can_id;
    s_response_can_id  = response_can_id;
    s_initialized      = true;
}

bool BootloaderClient_HandleFrame(uint32_t rx_id, const uint8_t *rx_data, uint8_t rx_len,
                                 uint8_t *reply_data, uint8_t *reply_len, uint32_t *reply_id,
                                 bool *trigger_reboot)
{
    if (!s_initialized || rx_len == 0 || rx_data == NULL) {
        return false;
    }

    /* Message must be addressed to this board specifically or to broadcast */
    if (rx_id != s_target_can_id && rx_id != CAN_ID_BOOT_BROADCAST) {
        return false;
    }

    uint8_t cmd = rx_data[0];
    *trigger_reboot = false;
    *reply_id = s_response_can_id;

    switch (cmd) {
        case CMD_ENTER_BOOTLOADER: {
            /* If payload provides a specific board target, verify match */
            if (rx_len >= 2) {
                uint8_t target_id = rx_data[1];
                if (target_id != s_board_id && target_id != BOARD_ID_ALL) {
                    return false;
                }
            }

            /* Respond with ACK before rebooting */
            if (reply_data != NULL) {
                reply_data[0] = CMD_ACK;
                reply_data[1] = CMD_ENTER_BOOTLOADER;
                reply_data[2] = BOOT_STATUS_OK;
                reply_data[3] = s_board_id;
                *reply_len = 4;
            }

            *trigger_reboot = true;
            return true;
        }

        case CMD_PING: {
            /* Respond with PING ACK indicating board is alive in App Mode */
            if (reply_data != NULL) {
                reply_data[0] = CMD_ACK;
                reply_data[1] = CMD_PING;
                reply_data[2] = BOOT_MODE_APP;       /* Mode: In Application */
                reply_data[3] = s_board_id;
                reply_data[4] = APP_FW_VERSION_MAJOR;
                reply_data[5] = APP_FW_VERSION_MINOR;
                reply_data[6] = APP_FW_VERSION_PATCH;
                reply_data[7] = 0x00;
                *reply_len = 8;
            }
            return true;
        }

        default:
            /* Other commands are handled only inside Bootloader Mode */
            return false;
    }
}

void BootloaderClient_EnterBootloader(void)
{
    /* Set magic token in dedicated SRAM location */
    volatile uint32_t *magic_reg = (volatile uint32_t *)BOOTLOADER_MAGIC_ADDR;
    *magic_reg = BOOTLOADER_MAGIC_KEY;

    /* Ensure memory write is committed before reset */
    __DSB();
    __ISB();

    /* Trigger MCU hardware reset */
    NVIC_SystemReset();
}

#if defined(HAL_FDCAN_MODULE_ENABLED)
bool BootloaderClient_ProcessFDCAN(FDCAN_HandleTypeDef *hfdcan,
                                   FDCAN_RxHeaderTypeDef *rx_header,
                                   const uint8_t *rx_data)
{
    if (hfdcan == NULL || rx_header == NULL || rx_data == NULL) {
        return false;
    }

    uint32_t rx_id = rx_header->Identifier;
    /* Convert DLC code to byte length (up to 8 bytes for command frames) */
    uint8_t rx_len = (rx_header->DataLength >> 16);
    if (rx_len == 0) {
        rx_len = 8; /* Fallback safe length */
    }

    uint8_t reply_data[8] = {0};
    uint8_t reply_len = 0;
    uint32_t reply_id = 0;
    bool trigger_reboot = false;

    bool handled = BootloaderClient_HandleFrame(rx_id, rx_data, rx_len,
                                                reply_data, &reply_len, &reply_id,
                                                &trigger_reboot);
    if (!handled) {
        return false;
    }

    /* Send response frame */
    if (reply_len > 0) {
        FDCAN_TxHeaderTypeDef tx_header;
        memset(&tx_header, 0, sizeof(tx_header));
        tx_header.Identifier = reply_id;
        tx_header.IdType = FDCAN_STANDARD_ID;
        tx_header.TxFrameType = FDCAN_DATA_FRAME;
        tx_header.DataLength = (reply_len <= 8) ? (reply_len << 16) : FDCAN_DLC_BYTES_8;
        tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
        tx_header.BitRateSwitch = FDCAN_BRS_OFF;
        tx_header.FDFormat = FDCAN_CLASSIC_CAN;
        tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
        tx_header.MessageMarker = 0;

        HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &tx_header, reply_data);
    }

    if (trigger_reboot) {
        /* Allow brief 10ms for CAN frame to transmit before resetting MCU */
        HAL_Delay(10);
        BootloaderClient_EnterBootloader();
    }

    return true;
}
#endif

#if defined(HAL_CAN_MODULE_ENABLED)
bool BootloaderClient_ProcessClassicCAN(CAN_HandleTypeDef *hcan,
                                        CAN_RxHeaderTypeDef *rx_header,
                                        const uint8_t *rx_data)
{
    if (hcan == NULL || rx_header == NULL || rx_data == NULL) {
        return false;
    }

    uint32_t rx_id = (rx_header->IDE == CAN_ID_STD) ? rx_header->StdId : rx_header->ExtId;
    uint8_t rx_len = rx_header->DLC;

    uint8_t reply_data[8] = {0};
    uint8_t reply_len = 0;
    uint32_t reply_id = 0;
    bool trigger_reboot = false;

    bool handled = BootloaderClient_HandleFrame(rx_id, rx_data, rx_len,
                                                reply_data, &reply_len, &reply_id,
                                                &trigger_reboot);
    if (!handled) {
        return false;
    }

    if (reply_len > 0) {
        CAN_TxHeaderTypeDef tx_header;
        uint32_t tx_mailbox;
        memset(&tx_header, 0, sizeof(tx_header));
        tx_header.StdId = reply_id;
        tx_header.IDE = CAN_ID_STD;
        tx_header.RTR = CAN_RTR_DATA;
        tx_header.DLC = reply_len;

        HAL_CAN_AddTxMessage(hcan, &tx_header, reply_data, &tx_mailbox);
    }

    if (trigger_reboot) {
        HAL_Delay(10);
        BootloaderClient_EnterBootloader();
    }

    return true;
}
#endif

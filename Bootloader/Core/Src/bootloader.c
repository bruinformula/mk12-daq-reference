/**
 * @file    bootloader.c
 * @brief   BFR CAN Bootloader Engine & State Machine Implementation
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#include "bootloader.h"
#include "fdcan.h"
#include "flash_if.h"
#include "crc32.h"
#include "led.h"

/* ARM Cortex-M Core Peripherals */
#define SCB_VTOR                    (*(volatile uint32_t *)0xE000ED08UL)
#define SYSTICK_CTRL                (*(volatile uint32_t *)0xE000E010UL)
#define SYSTICK_LOAD                (*(volatile uint32_t *)0xE000E014UL)
#define SYSTICK_VAL                 (*(volatile uint32_t *)0xE000E018UL)

#define NVIC_ICER_BASE              ((volatile uint32_t *)0xE000E180UL)
#define NVIC_ICPR_BASE              ((volatile uint32_t *)0xE000E280UL)

static uint32_t s_app_flashed_size = 0;
static bool     s_jump_requested   = false;

/* Streaming block buffer for Classic CAN 2.0 (96-byte blocks) */
static uint8_t  s_stream_buffer[CLASSIC_CAN_BLOCK_SIZE];
static uint32_t s_stream_offset       = 0;
static uint8_t  s_stream_expected_len = 0;
static uint8_t  s_stream_received_len = 0;

void Bootloader_Init(void)
{
    LED_Init();
    LED_On();

    FDCAN_Driver_Init();
    FDCAN_Driver_Start();
}

bool Bootloader_CheckAppValid(void)
{
    uint32_t *app_vectors = (uint32_t *)APPLICATION_START_ADDR;
    uint32_t sp = app_vectors[0];
    uint32_t reset_handler = app_vectors[1];

    /* 1. Initial Stack Pointer must point into valid SRAM range (0x20000000 - 0x20044000) */
    if (sp < SRAM_START_ADDR || sp > (SRAM_START_ADDR + SRAM_TOTAL_SIZE)) {
        return false;
    }

    /* 2. Reset vector must point into application flash partition and have Thumb bit (bit 0) set */
    if (reset_handler < APPLICATION_START_ADDR ||
        reset_handler >= (FLASH_BASE_ADDR + (FLASH_TOTAL_PAGES * FLASH_PAGE_SIZE)) ||
        (reset_handler & 0x01UL) == 0) {
        return false;
    }

    /* 3. Memory must not be blank erased (0xFFFFFFFF) */
    if (sp == 0xFFFFFFFFUL || reset_handler == 0xFFFFFFFFUL) {
        return false;
    }

    return true;
}

void Bootloader_JumpToApp(void)
{
    if (!Bootloader_CheckAppValid()) {
        return;
    }

    /* Turn off LED before handing over to app */
    LED_Off();

    /* Stop CAN controller */
    FDCAN_Driver_Stop();

    /* Disable all interrupts */
    __asm volatile ("cpsid i" : : : "memory");

    /* Disable and reset SysTick */
    SYSTICK_CTRL = 0;
    SYSTICK_LOAD = 0;
    SYSTICK_VAL  = 0;

    /* Clear and disable all NVIC interrupt channels */
    for (int i = 0; i < 8; i++) {
        NVIC_ICER_BASE[i] = 0xFFFFFFFFUL;
        NVIC_ICPR_BASE[i] = 0xFFFFFFFFUL;
    }

    /* Set Vector Table Offset Register (VTOR) to Application Flash Base */
    SCB_VTOR = APPLICATION_START_ADDR;

    /* Fetch Application initial Stack Pointer and Reset Handler */
    uint32_t *app_vectors = (uint32_t *)APPLICATION_START_ADDR;
    uint32_t app_sp = app_vectors[0];
    uint32_t app_reset = app_vectors[1];

    /* Set Main Stack Pointer (MSP) */
    __asm volatile ("msr msp, %0" : : "r" (app_sp) : );

    /* Re-enable interrupts */
    __asm volatile ("cpsie i" : : : "memory");

    /* Jump to application Reset Handler */
    void (*app_entry)(void) = (void (*)(void))app_reset;
    app_entry();

    /* Should never reach here */
    while (1);
}

bool Bootloader_ProcessMessage(uint32_t rx_id, const uint8_t *rx_data, uint8_t rx_len,
                              uint8_t *tx_data, uint8_t *tx_len, uint32_t *tx_id)
{
    if (rx_len == 0 || rx_data == NULL || tx_data == NULL) {
        return false;
    }

    /* Filter for our target ID or broadcast */
    if (rx_id != TARGET_CAN_ID && rx_id != BROADCAST_CAN_ID) {
        return false;
    }

    uint8_t cmd = rx_data[0];
    *tx_id = RESPONSE_CAN_ID;
    *tx_len = 0;

    switch (cmd) {
        case CMD_PING: {
            tx_data[0] = CMD_ACK;
            tx_data[1] = CMD_PING;
            tx_data[2] = BOOT_MODE_BOOTLOADER;
            tx_data[3] = TARGET_BOARD_ID;
            tx_data[4] = BOOTLOADER_VER_MAJOR;
            tx_data[5] = BOOTLOADER_VER_MINOR;
            tx_data[6] = BOOTLOADER_VER_PATCH;
            tx_data[7] = 0x00;
            *tx_len = 8;
            return true;
        }

        case CMD_ENTER_BOOTLOADER: {
            tx_data[0] = CMD_ACK;
            tx_data[1] = CMD_ENTER_BOOTLOADER;
            tx_data[2] = BOOT_STATUS_OK;
            tx_data[3] = TARGET_BOARD_ID;
            *tx_len = 4;
            return true;
        }

        case CMD_MODE_NEGOTIATE: {
            /* Inform host of CAN-FD capabilities */
            tx_data[0] = CMD_ACK;
            tx_data[1] = CMD_MODE_NEGOTIATE;
            tx_data[2] = 0x01; /* CAN-FD Supported */
            tx_data[3] = 64;   /* Max payload bytes */
            tx_data[4] = 0x01; /* Bit rate switch supported */
            tx_data[5] = 0x00;
            tx_data[6] = 0x00;
            tx_data[7] = 0x00;
            *tx_len = 8;
            return true;
        }

        case CMD_ERASE_APP: {
            /* Erase all application pages (Pages 4 through 31) */
            Flash_Unlock();
            Flash_Status_t status = Flash_ErasePages(APPLICATION_PAGE_START, APPLICATION_PAGE_END);
            Flash_Lock();

            s_app_flashed_size = 0;

            tx_data[0] = (status == FLASH_STATUS_OK) ? CMD_ACK : CMD_NACK;
            tx_data[1] = CMD_ERASE_APP;
            tx_data[2] = (status == FLASH_STATUS_OK) ? BOOT_STATUS_OK : BOOT_STATUS_ERROR;
            tx_data[3] = (uint8_t)status;
            *tx_len = 4;
            return true;
        }

        case CMD_WRITE_DATA: {
            /* Format:
             * Byte 0: CMD_WRITE_DATA (0x04)
             * Bytes 1..4: Target Flash Offset relative to APPLICATION_START_ADDR (Little Endian)
             * Bytes 5..rx_len-1: Data payload to write */
            if (rx_len <= 5) {
                tx_data[0] = CMD_NACK;
                tx_data[1] = CMD_WRITE_DATA;
                tx_data[2] = BOOT_STATUS_ERROR;
                *tx_len = 3;
                return true;
            }

            uint32_t offset = ((uint32_t)rx_data[1]) |
                              ((uint32_t)rx_data[2] << 8) |
                              ((uint32_t)rx_data[3] << 16) |
                              ((uint32_t)rx_data[4] << 24);

            uint32_t target_addr = APPLICATION_START_ADDR + offset;
            uint32_t payload_len = rx_len - 5;
            const uint8_t *payload = &rx_data[5];

            Flash_Unlock();
            Flash_Status_t status = Flash_WriteBuffer(target_addr, payload, payload_len);
            Flash_Lock();

            if (status == FLASH_STATUS_OK) {
                if ((offset + payload_len) > s_app_flashed_size) {
                    s_app_flashed_size = offset + payload_len;
                }
                /* Visual indicator during flashing */
                LED_Toggle();
            }

            tx_data[0] = (status == FLASH_STATUS_OK) ? CMD_ACK : CMD_NACK;
            tx_data[1] = CMD_WRITE_DATA;
            tx_data[2] = (status == FLASH_STATUS_OK) ? BOOT_STATUS_OK : BOOT_STATUS_ERROR;
            tx_data[3] = rx_data[1]; /* Echo back chunk offset byte for sequence tracking */
            *tx_len = 4;
            return true;
        }

        case CMD_STREAM_HEADER: {
            /* Classic CAN Block Header:
             * Byte 0: CMD_STREAM_HEADER (0x09)
             * Bytes 1..4: Target Flash Offset (Little Endian)
             * Byte 5: Expected Block Size (e.g. 96 bytes) */
            if (rx_len < 6) {
                tx_data[0] = CMD_NACK;
                tx_data[1] = CMD_STREAM_HEADER;
                tx_data[2] = BOOT_STATUS_ERROR;
                *tx_len = 3;
                return true;
            }

            s_stream_offset = ((uint32_t)rx_data[1]) |
                              ((uint32_t)rx_data[2] << 8) |
                              ((uint32_t)rx_data[3] << 16) |
                              ((uint32_t)rx_data[4] << 24);
            s_stream_expected_len = rx_data[5];
            s_stream_received_len = 0;

            tx_data[0] = CMD_ACK;
            tx_data[1] = CMD_STREAM_HEADER;
            tx_data[2] = BOOT_STATUS_OK;
            *tx_len = 3;
            return true;
        }

        case CMD_STREAM_DATA: {
            /* Classic CAN 8-byte streaming frames:
             * Appends bytes 1..rx_len-1 into s_stream_buffer */
            for (uint8_t i = 1; i < rx_len; i++) {
                if (s_stream_received_len < CLASSIC_CAN_BLOCK_SIZE) {
                    s_stream_buffer[s_stream_received_len++] = rx_data[i];
                }
            }

            /* Once block is complete, commit to flash and emit Block-ACK */
            if (s_stream_received_len >= s_stream_expected_len && s_stream_expected_len > 0) {
                uint32_t target_addr = APPLICATION_START_ADDR + s_stream_offset;
                Flash_Unlock();
                Flash_Status_t status = Flash_WriteBuffer(target_addr, s_stream_buffer, s_stream_received_len);
                Flash_Lock();

                if (status == FLASH_STATUS_OK) {
                    if ((s_stream_offset + s_stream_received_len) > s_app_flashed_size) {
                        s_app_flashed_size = s_stream_offset + s_stream_received_len;
                    }
                    LED_Toggle();
                }

                tx_data[0] = (status == FLASH_STATUS_OK) ? CMD_ACK : CMD_NACK;
                tx_data[1] = CMD_STREAM_DATA;
                tx_data[2] = (status == FLASH_STATUS_OK) ? BOOT_STATUS_OK : BOOT_STATUS_ERROR;
                tx_data[3] = (uint8_t)(s_stream_offset & 0xFF);
                *tx_len = 4;
                s_stream_expected_len = 0;
                s_stream_received_len = 0;
                return true;
            }

            return false; /* No ACK until full block is assembled */
        }

        case CMD_VERIFY: {
            /* Format:
             * Byte 0: CMD_VERIFY (0x05)
             * Bytes 1..4: Total Image Size (Little Endian)
             * Bytes 5..8: Expected CRC32 (Little Endian) */
            if (rx_len < 9) {
                tx_data[0] = CMD_NACK;
                tx_data[1] = CMD_VERIFY;
                tx_data[2] = BOOT_STATUS_ERROR;
                *tx_len = 3;
                return true;
            }

            uint32_t image_size = ((uint32_t)rx_data[1]) |
                                  ((uint32_t)rx_data[2] << 8) |
                                  ((uint32_t)rx_data[3] << 16) |
                                  ((uint32_t)rx_data[4] << 24);

            uint32_t expected_crc = ((uint32_t)rx_data[5]) |
                                    ((uint32_t)rx_data[6] << 8) |
                                    ((uint32_t)rx_data[7] << 16) |
                                    ((uint32_t)rx_data[8] << 24);

            if (image_size > APPLICATION_MAX_SIZE || image_size == 0) {
                tx_data[0] = CMD_NACK;
                tx_data[1] = CMD_VERIFY;
                tx_data[2] = BOOT_STATUS_INVALID_ADDR;
                *tx_len = 3;
                return true;
            }

            uint32_t computed_crc = CRC32_Calculate((const uint8_t *)APPLICATION_START_ADDR, image_size);

            if (computed_crc == expected_crc) {
                tx_data[0] = CMD_ACK;
                tx_data[1] = CMD_VERIFY;
                tx_data[2] = BOOT_STATUS_OK;
                /* Echo computed CRC */
                tx_data[3] = (uint8_t)(computed_crc & 0xFF);
                tx_data[4] = (uint8_t)((computed_crc >> 8) & 0xFF);
                tx_data[5] = (uint8_t)((computed_crc >> 16) & 0xFF);
                tx_data[6] = (uint8_t)((computed_crc >> 24) & 0xFF);
                *tx_len = 7;
            } else {
                tx_data[0] = CMD_NACK;
                tx_data[1] = CMD_VERIFY;
                tx_data[2] = BOOT_STATUS_CRC_FAIL;
                tx_data[3] = (uint8_t)(computed_crc & 0xFF);
                tx_data[4] = (uint8_t)((computed_crc >> 8) & 0xFF);
                tx_data[5] = (uint8_t)((computed_crc >> 16) & 0xFF);
                tx_data[6] = (uint8_t)((computed_crc >> 24) & 0xFF);
                *tx_len = 7;
            }
            return true;
        }

        case CMD_JUMP_APP: {
            if (Bootloader_CheckAppValid()) {
                tx_data[0] = CMD_ACK;
                tx_data[1] = CMD_JUMP_APP;
                tx_data[2] = BOOT_STATUS_OK;
                *tx_len = 3;
                s_jump_requested = true;
            } else {
                tx_data[0] = CMD_NACK;
                tx_data[1] = CMD_JUMP_APP;
                tx_data[2] = BOOT_STATUS_ERROR;
                *tx_len = 3;
            }
            return true;
        }

        default:
            return false;
    }
}

void Bootloader_Run(void)
{
    FDCAN_Message_t rx_msg;
    FDCAN_Message_t tx_msg;
    uint32_t blink_counter = 0;

    while (1) {
        /* Process any incoming CAN frames */
        if (FDCAN_Driver_Receive(&rx_msg)) {
            uint8_t tx_data[64] = {0};
            uint8_t tx_len = 0;
            uint32_t tx_id = 0;

            if (Bootloader_ProcessMessage(rx_msg.id, rx_msg.data, rx_msg.len,
                                         tx_data, &tx_len, &tx_id)) {
                if (tx_len > 0) {
                    tx_msg.id = tx_id;
                    tx_msg.len = tx_len;
                    /* Mirror the host's frame mode (FD vs Classic) */
                    tx_msg.is_fd = rx_msg.is_fd;
                    tx_msg.brs   = rx_msg.brs;
                    memcpy(tx_msg.data, tx_data, tx_len);

                    FDCAN_Driver_Transmit(&tx_msg);
                }

                if (s_jump_requested) {
                    /* Small delay so ACK frame completely leaves TX FIFO onto CAN bus */
                    for (volatile int i = 0; i < 50000; i++) { __asm volatile ("nop"); }
                    Bootloader_JumpToApp();
                }
            }
        }

        /* Periodic heartbeat blink when idle (approx 5 Hz) */
        if (++blink_counter >= 150000) {
            LED_Toggle();
            blink_counter = 0;
        }
    }
}

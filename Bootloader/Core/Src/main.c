/**
 * @file    main.c
 * @brief   BFR STM32U535 DAQ Bootloader - Main Entry Point
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#include "main.h"
#include "bootloader.h"
#include "fdcan.h"
#include "led.h"

void Error_Handler(void)
{
    /* Rapid error strobe on PB7 LED */
    while (1) {
        LED_Toggle();
        for (volatile int i = 0; i < 100000; i++) {
            __asm volatile ("nop");
        }
    }
}

int main(void)
{
    /* 1. Inspect SRAM Magic Token */
    volatile uint32_t *magic_reg = (volatile uint32_t *)BOOTLOADER_MAGIC_ADDR;
    bool force_bootloader = (*magic_reg == BOOTLOADER_MAGIC_KEY);
    *magic_reg = 0; /* Clear magic key so future power cycles boot normally */

    /* 2. Check if a valid application is already flashed at 0x08008000 */
    bool app_valid = Bootloader_CheckAppValid();

    /* 3. If application is corrupt/empty, force bootloader mode permanently */
    if (!app_valid) {
        force_bootloader = true;
    }

    /* 4. If neither condition forced bootloader, perform 250ms CAN discovery window */
    if (!force_bootloader) {
        LED_Init();
        LED_On();

        FDCAN_Driver_Init();
        FDCAN_Driver_Start();

        /* Listen for ~250ms for incoming host command */
        FDCAN_Message_t rx_msg;
        uint32_t wait_iterations = 250000; /* Approximate ~250ms at 16 MHz */

        while (wait_iterations > 0) {
            if (FDCAN_Driver_Receive(&rx_msg)) {
                if (rx_msg.id == TARGET_CAN_ID || rx_msg.id == BROADCAST_CAN_ID) {
                    uint8_t cmd = rx_msg.data[0];
                    if (cmd == CMD_ENTER_BOOTLOADER || cmd == CMD_PING) {
                        force_bootloader = true;
                        break;
                    }
                }
            }
            wait_iterations--;
        }

        /* If no host command arrived and app is valid, jump immediately to app */
        if (!force_bootloader) {
            Bootloader_JumpToApp();
        }
    }

    /* 5. Enter active bootloader session */
    Bootloader_Init();
    Bootloader_Run();

    return 0;
}

/**
 * @file    main.h
 * @brief   BFR DAQ CAN Bootloader - Main Configuration (STM32U535 & STM32L552)
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#ifndef INC_MAIN_H_
#define INC_MAIN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ========================================================================= */
/* Target MCU Hardware Configuration                                         */
/* ========================================================================= */

#if defined(STM32L552xx)

#define MCU_PART_NUMBER             "STM32L552CCT6"
#define MCU_CORE_CLOCK_HZ           16000000UL /* 16 MHz HSI default internal clock */

/* CAN Pins: FDCAN1 on PB8 (RX) and PB9 (TX) - Alternate Function AF9 */
#define CAN_RX_PORT                 GPIOB
#define CAN_RX_PIN                  8U
#define CAN_TX_PORT                 GPIOB
#define CAN_TX_PIN                  9U

/* Status LED: Disabled on Mk11 to avoid conflicting with PB7 I2C1_SDA */
#undef MCU_HAS_STATUS_LED

/* Memory Map Configuration (256 KB Flash, 2 KB Page Erase, Dual Bank) */
#define FLASH_BASE_ADDR             0x08000000UL
#define FLASH_PAGE_SIZE             2048UL       /* 2 KB page */
#define FLASH_TOTAL_PAGES           128U
#define FLASH_PAGES_PER_BANK        64U

/* Bootloader Partition: Pages 0–15 (32 KB) - STRICTLY PROTECTED */
#define BOOTLOADER_START_ADDR       0x08000000UL
#define BOOTLOADER_SIZE             0x00008000UL /* 32 KB */
#define BOOTLOADER_PAGES_COUNT      16U

/* Application Partition: Pages 16–127 (224 KB) */
#define APPLICATION_START_ADDR      0x08008000UL
#define APPLICATION_PAGE_START      16U
#define APPLICATION_PAGE_END        127U
#define APPLICATION_MAX_SIZE        (224UL * 1024UL) /* 224 KB */

/* SRAM Map (256 KB SRAM) */
#define SRAM_START_ADDR             0x20000000UL
#define SRAM_TOTAL_SIZE             (256UL * 1024UL) /* 256 KB */
#define BOOTLOADER_MAGIC_ADDR       0x20000000UL
#define BOOTLOADER_MAGIC_KEY        0xDEADBEEFUL

#else /* Default: STM32U535xx */

#define MCU_PART_NUMBER             "STM32U535CCT6"
#define MCU_CORE_CLOCK_HZ           16000000UL /* 16 MHz HSI default internal clock */

/* Status LED: PB7 drives NPN Q4 (MMBT3904) pulling D18 (Red) cathode to GND */
#define MCU_HAS_STATUS_LED          1
#define STATUS_LED_PORT             GPIOB
#define STATUS_LED_PIN              7U

/* CAN Pins: FDCAN1 on PB8 (RX) and PB9 (TX) - Alternate Function AF9 */
#define CAN_RX_PORT                 GPIOB
#define CAN_RX_PIN                  8U
#define CAN_TX_PORT                 GPIOB
#define CAN_TX_PIN                  9U

/* Memory Map Configuration (256 KB Flash, 8 KB Page Erase, Single Bank) */
#define FLASH_BASE_ADDR             0x08000000UL
#define FLASH_PAGE_SIZE             8192UL       /* 8 KB page */
#define FLASH_TOTAL_PAGES           32U

/* Bootloader Partition: Pages 0–3 (32 KB) - STRICTLY PROTECTED */
#define BOOTLOADER_START_ADDR       0x08000000UL
#define BOOTLOADER_SIZE             0x00008000UL /* 32 KB */
#define BOOTLOADER_PAGES_COUNT      4U

/* Application Partition: Pages 4–31 (224 KB) */
#define APPLICATION_START_ADDR      0x08008000UL
#define APPLICATION_PAGE_START      4U
#define APPLICATION_PAGE_END        31U
#define APPLICATION_MAX_SIZE        (224UL * 1024UL) /* 224 KB */

/* SRAM Map (272 KB SRAM) */
#define SRAM_START_ADDR             0x20000000UL
#define SRAM_TOTAL_SIZE             (272UL * 1024UL) /* 272 KB */
#define BOOTLOADER_MAGIC_ADDR       0x20000000UL
#define BOOTLOADER_MAGIC_KEY        0xDEADBEEFUL

#endif

/* ========================================================================= */
/* Board Identity & CAN IDs                                                 */
/* ========================================================================= */

#ifndef TARGET_BOARD_ID
#define TARGET_BOARD_ID             0x03 /* Default: SDU */
#endif

#ifndef TARGET_CAN_ID
#define TARGET_CAN_ID               0x014UL /* Default: Host -> SDU */
#endif

#ifndef RESPONSE_CAN_ID
#define RESPONSE_CAN_ID             0x015UL /* Default: SDU -> Host */
#endif

#define BROADCAST_CAN_ID            0x07FUL

/* Bootloader Firmware Version */
#define BOOTLOADER_VER_MAJOR        1U
#define BOOTLOADER_VER_MINOR        1U
#define BOOTLOADER_VER_PATCH        0U

/* Timing */
#define BOOT_WAIT_TIMEOUT_MS        250U

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_MAIN_H_ */

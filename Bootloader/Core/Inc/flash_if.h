/**
 * @file    flash_if.h
 * @brief   Dual-target Flash Driver Interface (STM32U535 Quad-Word & STM32L552 Double-Word)
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#ifndef INC_FLASH_IF_H_
#define INC_FLASH_IF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#if defined(STM32L552xx)
#define FLASH_QUADWORD_SIZE         8UL  /* 64-bit double-word write granularity */
#else
#define FLASH_QUADWORD_SIZE         16UL /* 128-bit quad-word write granularity */
#endif

#ifndef QUADWORD_SIZE
#define QUADWORD_SIZE               FLASH_QUADWORD_SIZE
#endif

typedef enum {
    FLASH_STATUS_OK = 0,
    FLASH_STATUS_ERROR,
    FLASH_STATUS_BUSY,
    FLASH_STATUS_TIMEOUT,
    FLASH_STATUS_PROTECTED
} Flash_Status_t;

/**
 * @brief Unlock the FLASH control register access.
 */
Flash_Status_t Flash_Unlock(void);

/**
 * @brief Lock the FLASH control register.
 */
Flash_Status_t Flash_Lock(void);

/**
 * @brief Erase a range of application pages.
 *        Bootloader pages are strictly protected.
 * @param start_page Starting page index
 * @param end_page   Ending page index
 */
Flash_Status_t Flash_ErasePages(uint32_t start_page, uint32_t end_page);

/**
 * @brief Program a basic flash unit (128-bit quad-word on U5, 64-bit double-word on L5).
 * @param address Aligned address in application flash (>= 0x08008000)
 * @param data    Pointer to 32-bit words
 */
Flash_Status_t Flash_ProgramQuadWord(uint32_t address, const uint32_t *data);

/**
 * @brief Program arbitrary length buffer (automatically pads last word unit with 0xFF).
 * @param address Address in application flash (>= 0x08008000)
 * @param buffer  Data buffer to write
 * @param length  Length in bytes
 */
Flash_Status_t Flash_WriteBuffer(uint32_t address, const uint8_t *buffer, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* INC_FLASH_IF_H_ */

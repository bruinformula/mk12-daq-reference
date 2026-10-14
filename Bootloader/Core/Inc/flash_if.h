/**
 * @file    flash_if.h
 * @brief   STM32U5 Flash Interface (8 KB Page Erase, 128-bit Quad-Word Write)
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#ifndef INC_FLASH_IF_H_
#define INC_FLASH_IF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define FLASH_QUADWORD_SIZE         16UL
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
 * @brief Erase a range of application pages (Pages 4..31).
 *        Pages 0..3 are STRICTLY PROTECTED and will return FLASH_STATUS_PROTECTED.
 * @param start_page Starting page index (minimum 4)
 * @param end_page   Ending page index (maximum 31)
 */
Flash_Status_t Flash_ErasePages(uint32_t start_page, uint32_t end_page);

/**
 * @brief Program a 128-bit (16-byte) quad-word into flash memory.
 * @param address 16-byte aligned address in application flash (>= 0x08008000)
 * @param data    Pointer to 4 x 32-bit words (16 bytes)
 */
Flash_Status_t Flash_ProgramQuadWord(uint32_t address, const uint32_t *data);

/**
 * @brief Program arbitrary length buffer (automatically pads last quad-word if needed).
 * @param address Address in application flash (>= 0x08008000)
 * @param buffer  Data buffer to write
 * @param length  Length in bytes
 */
Flash_Status_t Flash_WriteBuffer(uint32_t address, const uint8_t *buffer, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* INC_FLASH_IF_H_ */

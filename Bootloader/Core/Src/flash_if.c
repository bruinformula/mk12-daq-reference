/**
 * @file    flash_if.c
 * @brief   STM32U5 Flash Driver (8 KB Page Erase, 128-bit Quad-Word Programming)
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#include "flash_if.h"

/* ========================================================================= */
/* STM32U5 Flash Controller Hardware Registers                               */
/* ========================================================================= */

#define FLASH_REG_BASE              0x40022000UL

#define FLASH_NSKEYR                (*(volatile uint32_t *)(FLASH_REG_BASE + 0x08UL))
#define FLASH_NSSR                  (*(volatile uint32_t *)(FLASH_REG_BASE + 0x20UL))
#define FLASH_NSCR                  (*(volatile uint32_t *)(FLASH_REG_BASE + 0x28UL))

/* Flash Unlock Keys */
#define FLASH_KEY1                  0x45670123UL
#define FLASH_KEY2                  0xCDEF89ABUL

/* Flash Status Register Bits */
#define FLASH_SR_BSY                (1UL << 0)
#define FLASH_SR_CFGBSY             (1UL << 1)
#define FLASH_SR_EOP                (1UL << 3)
#define FLASH_SR_WRPERR             (1UL << 4)
#define FLASH_SR_PGSERR             (1UL << 5)
#define FLASH_SR_STRBERR            (1UL << 6)
#define FLASH_SR_INCERR             (1UL << 7)
#define FLASH_SR_ERR_ALL            (FLASH_SR_WRPERR | FLASH_SR_PGSERR | FLASH_SR_STRBERR | FLASH_SR_INCERR)

/* Flash Control Register Bits */
#define FLASH_CR_PG                 (1UL << 0)
#define FLASH_CR_PER                (1UL << 1)
#define FLASH_CR_PNB_POS            3U
#define FLASH_CR_PNB_MSK            (0x3FUL << FLASH_CR_PNB_POS)
#define FLASH_CR_STRT               (1UL << 16)
#define FLASH_CR_LOCK               (1UL << 31)

#define FLASH_TIMEOUT_COUNT         10000000UL

static Flash_Status_t Flash_WaitForLastOperation(void)
{
    uint32_t timeout = FLASH_TIMEOUT_COUNT;
    while ((FLASH_NSSR & (FLASH_SR_BSY | FLASH_SR_CFGBSY)) != 0) {
        if (--timeout == 0) {
            return FLASH_STATUS_TIMEOUT;
        }
    }

    /* Check error flags */
    if ((FLASH_NSSR & FLASH_SR_ERR_ALL) != 0) {
        /* Clear error flags by writing 1 */
        FLASH_NSSR |= FLASH_SR_ERR_ALL;
        return FLASH_STATUS_ERROR;
    }

    return FLASH_STATUS_OK;
}

Flash_Status_t Flash_Unlock(void)
{
    if ((FLASH_NSCR & FLASH_CR_LOCK) != 0) {
        FLASH_NSKEYR = FLASH_KEY1;
        FLASH_NSKEYR = FLASH_KEY2;
    }

    if ((FLASH_NSCR & FLASH_CR_LOCK) != 0) {
        return FLASH_STATUS_ERROR;
    }

    return FLASH_STATUS_OK;
}

Flash_Status_t Flash_Lock(void)
{
    FLASH_NSCR |= FLASH_CR_LOCK;
    return FLASH_STATUS_OK;
}

Flash_Status_t Flash_ErasePages(uint32_t start_page, uint32_t end_page)
{
    /* Failsafe: Never erase bootloader pages (Pages 0..3) */
    if (start_page < APPLICATION_PAGE_START || end_page > APPLICATION_PAGE_END || start_page > end_page) {
        return FLASH_STATUS_PROTECTED;
    }

    Flash_Status_t status = Flash_WaitForLastOperation();
    if (status != FLASH_STATUS_OK) {
        return status;
    }

    /* Clear any pending flags */
    FLASH_NSSR |= FLASH_SR_ERR_ALL | FLASH_SR_EOP;

    for (uint32_t page = start_page; page <= end_page; page++) {
        /* Configure Page Erase with target Page Number */
        uint32_t cr = FLASH_NSCR;
        cr &= ~(FLASH_CR_PNB_MSK | FLASH_CR_PG);
        cr |= FLASH_CR_PER | ((page & 0x3FUL) << FLASH_CR_PNB_POS);
        FLASH_NSCR = cr;

        /* Start erase operation */
        FLASH_NSCR |= FLASH_CR_STRT;

        /* Wait for completion */
        status = Flash_WaitForLastOperation();

        /* Clear PER bit */
        FLASH_NSCR &= ~FLASH_CR_PER;

        if (status != FLASH_STATUS_OK) {
            return status;
        }
    }

    return FLASH_STATUS_OK;
}

Flash_Status_t Flash_ProgramQuadWord(uint32_t address, const uint32_t *data)
{
    /* Address must be 16-byte aligned and inside application partition */
    if (address < APPLICATION_START_ADDR || (address + QUADWORD_SIZE) > (FLASH_BASE_ADDR + (FLASH_TOTAL_PAGES * FLASH_PAGE_SIZE))) {
        return FLASH_STATUS_PROTECTED;
    }

    if ((address & 0x0FUL) != 0) {
        return FLASH_STATUS_ERROR; /* Must be 128-bit aligned */
    }

    Flash_Status_t status = Flash_WaitForLastOperation();
    if (status != FLASH_STATUS_OK) {
        return status;
    }

    /* Clear pending flags */
    FLASH_NSSR |= FLASH_SR_ERR_ALL | FLASH_SR_EOP;

    /* Set programming mode */
    FLASH_NSCR |= FLASH_CR_PG;

    /* Write 4 consecutive 32-bit words (128 bits) */
    volatile uint32_t *dest = (volatile uint32_t *)address;
    dest[0] = data[0];
    dest[1] = data[1];
    dest[2] = data[2];
    dest[3] = data[3];

    /* Wait for flash operation to complete */
    status = Flash_WaitForLastOperation();

    /* Disable programming mode */
    FLASH_NSCR &= ~FLASH_CR_PG;

    return status;
}

Flash_Status_t Flash_WriteBuffer(uint32_t address, const uint8_t *buffer, uint32_t length)
{
    if (buffer == NULL || length == 0) {
        return FLASH_STATUS_OK;
    }

    uint32_t current_addr = address;
    uint32_t bytes_remaining = length;
    const uint8_t *src = buffer;

    while (bytes_remaining > 0) {
        uint32_t quad[4];
        if (bytes_remaining >= QUADWORD_SIZE) {
            memcpy(quad, src, QUADWORD_SIZE);
            src += QUADWORD_SIZE;
            bytes_remaining -= QUADWORD_SIZE;
        } else {
            /* Partial quad-word at end of image: fill remainder with 0xFF */
            memset(quad, 0xFF, QUADWORD_SIZE);
            memcpy(quad, src, bytes_remaining);
            src += bytes_remaining;
            bytes_remaining = 0;
        }

        Flash_Status_t status = Flash_ProgramQuadWord(current_addr, quad);
        if (status != FLASH_STATUS_OK) {
            return status;
        }

        current_addr += QUADWORD_SIZE;
    }

    return FLASH_STATUS_OK;
}

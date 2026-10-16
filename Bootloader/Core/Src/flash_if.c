/**
 * @file    flash_if.c
 * @brief   Dual-target Flash Driver (STM32U5 8KB/128-bit & STM32L5 2KB/64-bit Dual-Bank)
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#include "flash_if.h"

/* ========================================================================= */
/* Flash Controller Hardware Registers                                       */
/* ========================================================================= */

#define FLASH_REG_BASE              0x40022000UL

#define FLASH_NSKEYR                (*(volatile uint32_t *)(FLASH_REG_BASE + 0x08UL))
#define FLASH_NSSR                  (*(volatile uint32_t *)(FLASH_REG_BASE + 0x20UL))
#define FLASH_NSCR                  (*(volatile uint32_t *)(FLASH_REG_BASE + 0x28UL))

/* Flash Unlock Keys */
#define FLASH_KEY1                  0x45670123UL
#define FLASH_KEY2                  0xCDEF89ABUL

#if defined(STM32L552xx)
/* STM32L5 Status Register Bits */
#define FLASH_SR_BSY                (1UL << 16) /* NSBSY */
#define FLASH_SR_CFGBSY             0UL
#define FLASH_SR_EOP                (1UL << 0)  /* NSEOP */
#define FLASH_SR_ERR_ALL            ((1UL << 1) | (1UL << 3) | (1UL << 4) | (1UL << 5) | (1UL << 6) | (1UL << 7))

/* STM32L5 Control Register Bits */
#define FLASH_CR_PG                 (1UL << 0)
#define FLASH_CR_PER                (1UL << 1)
#define FLASH_CR_PNB_POS            3U
#define FLASH_CR_PNB_MSK            (0x7FUL << FLASH_CR_PNB_POS)
#define FLASH_CR_BKER               (1UL << 11)
#define FLASH_CR_STRT               (1UL << 16)
#define FLASH_CR_LOCK               (1UL << 31)

#else
/* STM32U5 Status Register Bits */
#define FLASH_SR_BSY                (1UL << 0)
#define FLASH_SR_CFGBSY             (1UL << 1)
#define FLASH_SR_EOP                (1UL << 3)
#define FLASH_SR_WRPERR             (1UL << 4)
#define FLASH_SR_PGSERR             (1UL << 5)
#define FLASH_SR_STRBERR            (1UL << 6)
#define FLASH_SR_INCERR             (1UL << 7)
#define FLASH_SR_ERR_ALL            (FLASH_SR_WRPERR | FLASH_SR_PGSERR | FLASH_SR_STRBERR | FLASH_SR_INCERR)

/* STM32U5 Control Register Bits */
#define FLASH_CR_PG                 (1UL << 0)
#define FLASH_CR_PER                (1UL << 1)
#define FLASH_CR_PNB_POS            3U
#define FLASH_CR_PNB_MSK            (0x3FUL << FLASH_CR_PNB_POS)
#define FLASH_CR_STRT               (1UL << 16)
#define FLASH_CR_LOCK               (1UL << 31)
#endif

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
    /* Failsafe: Never erase bootloader pages */
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
        uint32_t cr = FLASH_NSCR;
        cr &= ~(FLASH_CR_PNB_MSK | FLASH_CR_PG);

#if defined(STM32L552xx)
        /* Handle dual-bank page addressing on STM32L5 */
        if (page >= FLASH_PAGES_PER_BANK) {
            uint32_t b2_page = page - FLASH_PAGES_PER_BANK;
            cr |= FLASH_CR_PER | FLASH_CR_BKER | ((b2_page & 0x7FUL) << FLASH_CR_PNB_POS);
        } else {
            cr &= ~FLASH_CR_BKER;
            cr |= FLASH_CR_PER | ((page & 0x7FUL) << FLASH_CR_PNB_POS);
        }
#else
        cr |= FLASH_CR_PER | ((page & 0x3FUL) << FLASH_CR_PNB_POS);
#endif
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
    /* Address must be unit aligned and inside application partition */
    if (address < APPLICATION_START_ADDR || (address + QUADWORD_SIZE) > (FLASH_BASE_ADDR + (FLASH_TOTAL_PAGES * FLASH_PAGE_SIZE))) {
        return FLASH_STATUS_PROTECTED;
    }

    if ((address & (QUADWORD_SIZE - 1)) != 0) {
        return FLASH_STATUS_ERROR; /* Must be unit aligned */
    }

    Flash_Status_t status = Flash_WaitForLastOperation();
    if (status != FLASH_STATUS_OK) {
        return status;
    }

    /* Clear pending flags */
    FLASH_NSSR |= FLASH_SR_ERR_ALL | FLASH_SR_EOP;

    /* Set programming mode */
    FLASH_NSCR |= FLASH_CR_PG;

    volatile uint32_t *dest = (volatile uint32_t *)address;

#if defined(STM32L552xx)
    /* 64-bit double-word write */
    dest[0] = data[0];
    dest[1] = data[1];
#else
    /* 128-bit quad-word write */
    dest[0] = data[0];
    dest[1] = data[1];
    dest[2] = data[2];
    dest[3] = data[3];
#endif

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
            /* Partial word unit at end of image: fill remainder with 0xFF */
            memset(quad, 0xFF, sizeof(quad));
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

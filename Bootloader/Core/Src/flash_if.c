/**
 * @file    flash_if.c
 * @brief   Dual-target Flash Driver (STM32U5 8KB/128-bit & STM32L5 2KB/64-bit Dual-Bank)
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#include "flash_if.h"

/* ========================================================================= */
/* Memory Barriers (bare-metal Cortex-M33 without CMSIS header)              */
/* ========================================================================= */

#ifndef __DSB
#define __DSB()                     __asm volatile ("dsb 0xF" ::: "memory")
#endif
#ifndef __ISB
#define __ISB()                     __asm volatile ("isb 0xF" ::: "memory")
#endif

/* ========================================================================= */
/* Flash Controller Hardware Registers                                       */
/* ========================================================================= */

#define FLASH_REG_BASE              0x40022000UL /* FLASH_R_BASE_NS on both L5 & U5 */

#define FLASH_NSKEYR                (*(volatile uint32_t *)(FLASH_REG_BASE + 0x08UL))
#define FLASH_NSSR                  (*(volatile uint32_t *)(FLASH_REG_BASE + 0x20UL))
#define FLASH_NSCR                  (*(volatile uint32_t *)(FLASH_REG_BASE + 0x28UL))

/* Flash Unlock Keys */
#define FLASH_KEY1                  0x45670123UL
#define FLASH_KEY2                  0xCDEF89ABUL

/* Fallback if main.h does not yet define FLASH_PAGES_PER_BANK for STM32U535xx */
#ifndef FLASH_PAGES_PER_BANK
#if defined(STM32L552xx)
#define FLASH_PAGES_PER_BANK        64U
#else
#define FLASH_PAGES_PER_BANK        16U
#endif
#endif

#if defined(STM32L552xx)
/* STM32L5 FLASH_NSSR Status Register Bits (RM0438 / stm32l552xx.h) */
#define FLASH_SR_EOP                (1UL << 0)  /* NSEOP */
#define FLASH_SR_OPERR              (1UL << 1)  /* NSOPERR */
#define FLASH_SR_PROGERR            (1UL << 3)  /* NSPROGERR */
#define FLASH_SR_WRPERR             (1UL << 4)  /* NSWRPERR */
#define FLASH_SR_PGAERR             (1UL << 5)  /* NSPGAERR */
#define FLASH_SR_SIZERR             (1UL << 6)  /* NSSIZERR */
#define FLASH_SR_PGSERR             (1UL << 7)  /* NSPGSERR */
#define FLASH_SR_OPTWERR            (1UL << 13) /* OPTWERR */
#define FLASH_SR_BSY                (1UL << 16) /* NSBSY */
#define FLASH_SR_WAIT_MSK           FLASH_SR_BSY
#define FLASH_SR_ERR_ALL            (FLASH_SR_OPERR | FLASH_SR_PROGERR | FLASH_SR_WRPERR | \
                                     FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR | \
                                     FLASH_SR_OPTWERR)

/* STM32L5 FLASH_NSCR Control Register Bits (RM0438 / stm32l552xx.h) */
#define FLASH_CR_PG                 (1UL << 0)
#define FLASH_CR_PER                (1UL << 1)
#define FLASH_CR_PNB_POS            3U
#define FLASH_CR_PNB_MSK            (0x7FUL << FLASH_CR_PNB_POS)
#define FLASH_CR_BKER               (1UL << 11)
#define FLASH_CR_STRT               (1UL << 16)
#define FLASH_CR_LOCK               (1UL << 31)

#else
/* STM32U5 FLASH_NSSR Status Register Bits (RM0456 / stm32u535xx.h) */
#define FLASH_SR_EOP                (1UL << 0)  /* EOP */
#define FLASH_SR_OPERR              (1UL << 1)  /* OPERR */
#define FLASH_SR_PROGERR            (1UL << 3)  /* PROGERR */
#define FLASH_SR_WRPERR             (1UL << 4)  /* WRPERR */
#define FLASH_SR_PGAERR             (1UL << 5)  /* PGAERR */
#define FLASH_SR_SIZERR             (1UL << 6)  /* SIZERR */
#define FLASH_SR_PGSERR             (1UL << 7)  /* PGSERR */
#define FLASH_SR_OPTWERR            (1UL << 13) /* OPTWERR */
#define FLASH_SR_BSY                (1UL << 16) /* BSY */
#define FLASH_SR_WDW                (1UL << 17) /* WDW (Wait Data to Write) */
#define FLASH_SR_WAIT_MSK           (FLASH_SR_BSY | FLASH_SR_WDW)
#define FLASH_SR_ERR_ALL            (FLASH_SR_OPERR | FLASH_SR_PROGERR | FLASH_SR_WRPERR | \
                                     FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR | \
                                     FLASH_SR_OPTWERR)

/* STM32U5 FLASH_NSCR Control Register Bits (RM0456 / stm32u535xx.h) */
#define FLASH_CR_PG                 (1UL << 0)
#define FLASH_CR_PER                (1UL << 1)
#define FLASH_CR_PNB_POS            3U
#define FLASH_CR_PNB_MSK            (0x1FUL << FLASH_CR_PNB_POS)
#define FLASH_CR_BKER               (1UL << 11)
#define FLASH_CR_STRT               (1UL << 16)
#define FLASH_CR_LOCK               (1UL << 31)
#endif

#define FLASH_TIMEOUT_COUNT         10000000UL

static Flash_Status_t Flash_WaitForLastOperation(void)
{
    uint32_t timeout = FLASH_TIMEOUT_COUNT;
    while ((FLASH_NSSR & FLASH_SR_WAIT_MSK) != 0U) {
        if (--timeout == 0U) {
            return FLASH_STATUS_TIMEOUT;
        }
    }

    /* Check error flags */
    uint32_t sr_err = FLASH_NSSR & FLASH_SR_ERR_ALL;
    if (sr_err != 0U) {
        /* Clear error flags by direct write-1-to-clear (rc_w1) */
        FLASH_NSSR = sr_err;
        return FLASH_STATUS_ERROR;
    }

    /* Clear End of Operation flag if set (rc_w1) */
    if ((FLASH_NSSR & FLASH_SR_EOP) != 0U) {
        FLASH_NSSR = FLASH_SR_EOP;
    }

    return FLASH_STATUS_OK;
}

Flash_Status_t Flash_Unlock(void)
{
    if ((FLASH_NSCR & FLASH_CR_LOCK) != 0U) {
        FLASH_NSKEYR = FLASH_KEY1;
        FLASH_NSKEYR = FLASH_KEY2;
        __DSB();
    }

    if ((FLASH_NSCR & FLASH_CR_LOCK) != 0U) {
        return FLASH_STATUS_ERROR;
    }

    return FLASH_STATUS_OK;
}

Flash_Status_t Flash_Lock(void)
{
    FLASH_NSCR |= FLASH_CR_LOCK;
    __DSB();
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

    /* Clear any pending flags (rc_w1: direct write, not RMW) */
    FLASH_NSSR = FLASH_SR_ERR_ALL | FLASH_SR_EOP;

    for (uint32_t page = start_page; page <= end_page; page++) {
        uint32_t cr = FLASH_NSCR;
        cr &= ~(FLASH_CR_PNB_MSK | FLASH_CR_PG | FLASH_CR_BKER);

        /* Dual-bank page addressing (64 pages/bank on L552, 16 pages/bank on U535) */
        if (page >= FLASH_PAGES_PER_BANK) {
            uint32_t b2_page = page - FLASH_PAGES_PER_BANK;
            cr |= FLASH_CR_PER | FLASH_CR_BKER | ((b2_page << FLASH_CR_PNB_POS) & FLASH_CR_PNB_MSK);
        } else {
            cr |= FLASH_CR_PER | ((page << FLASH_CR_PNB_POS) & FLASH_CR_PNB_MSK);
        }
        FLASH_NSCR = cr;
        __DSB();

        /* Start erase operation */
        FLASH_NSCR |= FLASH_CR_STRT;
        __DSB();
        __ISB();

        /* Wait for completion */
        status = Flash_WaitForLastOperation();

        /* Clear PER and BKER bits */
        FLASH_NSCR &= ~(FLASH_CR_PER | FLASH_CR_BKER);
        __DSB();

        if (status != FLASH_STATUS_OK) {
            return status;
        }
    }

    return FLASH_STATUS_OK;
}

Flash_Status_t Flash_ProgramQuadWord(uint32_t address, const uint32_t *data)
{
    if (data == NULL) {
        return FLASH_STATUS_ERROR;
    }

    /* Address must be unit aligned and inside application partition */
    uint32_t flash_end = FLASH_BASE_ADDR + (FLASH_TOTAL_PAGES * FLASH_PAGE_SIZE);
    if (address < APPLICATION_START_ADDR || address > (flash_end - FLASH_QUADWORD_SIZE)) {
        return FLASH_STATUS_PROTECTED;
    }

    if ((address & (FLASH_QUADWORD_SIZE - 1UL)) != 0UL) {
        return FLASH_STATUS_ERROR; /* Must be unit aligned */
    }

    Flash_Status_t status = Flash_WaitForLastOperation();
    if (status != FLASH_STATUS_OK) {
        return status;
    }

    /* Clear pending flags (rc_w1: direct write, not RMW) */
    FLASH_NSSR = FLASH_SR_ERR_ALL | FLASH_SR_EOP;

    /* Set programming mode */
    FLASH_NSCR |= FLASH_CR_PG;
    __DSB();
    __ISB();

    volatile uint32_t *dest = (volatile uint32_t *)address;
    uint32_t primask;
    __asm volatile ("mrs %0, primask" : "=r" (primask) :: "memory");
    __asm volatile ("cpsid i" ::: "memory");

#if defined(STM32L552xx)
    /* 64-bit double-word write */
    dest[0] = data[0];
    __ISB();
    dest[1] = data[1];
#else
    /* 128-bit quad-word write */
    dest[0] = data[0];
    __ISB();
    dest[1] = data[1];
    __ISB();
    dest[2] = data[2];
    __ISB();
    dest[3] = data[3];
#endif
    __DSB();
    __ISB();

    __asm volatile ("msr primask, %0" :: "r" (primask) : "memory");

    /* Wait for flash operation to complete */
    status = Flash_WaitForLastOperation();

    /* Disable programming mode */
    FLASH_NSCR &= ~FLASH_CR_PG;
    __DSB();

    return status;
}

Flash_Status_t Flash_WriteBuffer(uint32_t address, const uint8_t *buffer, uint32_t length)
{
    if (buffer == NULL || length == 0U) {
        return FLASH_STATUS_OK;
    }

    uint32_t current_addr = address;
    uint32_t bytes_remaining = length;
    const uint8_t *src = buffer;

    while (bytes_remaining > 0U) {
        uint32_t quad[4];
        if (bytes_remaining >= FLASH_QUADWORD_SIZE) {
            memcpy(quad, src, FLASH_QUADWORD_SIZE);
            src += FLASH_QUADWORD_SIZE;
            bytes_remaining -= FLASH_QUADWORD_SIZE;
        } else {
            /* Partial word unit at end of image: fill remainder with 0xFF */
            memset(quad, 0xFF, sizeof(quad));
            memcpy(quad, src, bytes_remaining);
            src += bytes_remaining;
            bytes_remaining = 0U;
        }

        Flash_Status_t status = Flash_ProgramQuadWord(current_addr, quad);
        if (status != FLASH_STATUS_OK) {
            return status;
        }

        current_addr += FLASH_QUADWORD_SIZE;
    }

    return FLASH_STATUS_OK;
}

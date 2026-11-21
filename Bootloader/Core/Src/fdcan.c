/**
 * @file    fdcan.c
 * @brief   Bosch M_CAN / FDCAN1 Driver Implementation (PB8 RX, PB9 TX) for STM32U5 & STM32L5
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#include "fdcan.h"

/* ========================================================================= */
/* Peripheral & Register Base Addresses                                      */
/* ========================================================================= */

#if defined(STM32L552xx)
#define RCC_BASE_ADDR               0x40021000UL
#define RCC_CR                      (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x00UL))
#define RCC_CFGR                    (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x08UL))
#define RCC_PLLCFGR                 (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x0CUL))
#define RCC_AHB2ENR                 (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x4CUL))
#define RCC_APB1ENR2                (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x5CUL))
#define RCC_CCIPR1                  (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x88UL))
#else /* STM32U535xx */
#define PWR_BASE_ADDR               0x46020800UL
#define PWR_VOSR                    (*(volatile uint32_t *)(PWR_BASE_ADDR + 0x0CUL))
#define PWR_SVMSR                   (*(volatile uint32_t *)(PWR_BASE_ADDR + 0x3CUL))

#define RCC_BASE_ADDR               0x46020C00UL
#define RCC_CR                      (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x000UL))
#define RCC_CFGR1                   (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x01CUL))
#define RCC_PLL1CFGR                (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x028UL))
#define RCC_PLL1DIVR                (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x034UL))
#define RCC_AHB2ENR1                (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x08CUL))
#define RCC_AHB3ENR                 (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x094UL))
#define RCC_APB1ENR2                (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x0A0UL))
#define RCC_CCIPR1                  (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x0E0UL))
#endif

#define GPIOB_BASE_ADDR             0x42020400UL
#define GPIOB_MODER                 (*(volatile uint32_t *)(GPIOB_BASE_ADDR + 0x00UL))
#define GPIOB_OSPEEDR               (*(volatile uint32_t *)(GPIOB_BASE_ADDR + 0x08UL))
#define GPIOB_PUPDR                 (*(volatile uint32_t *)(GPIOB_BASE_ADDR + 0x0CUL))
#define GPIOB_AFRH                  (*(volatile uint32_t *)(GPIOB_BASE_ADDR + 0x24UL))

#define FDCAN1_BASE_ADDR            0x4000A400UL
#define FDCAN1_MSG_RAM_BASE         0x4000AC00UL

/* FDCAN1 Registers (STM32L5/U5 Simplified Bosch M_CAN) */
#define FDCAN_DBTP                  (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0x00CUL))
#define FDCAN_CCCR                  (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0x018UL))
#define FDCAN_NBTP                  (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0x01CUL))
#define FDCAN_RXGFC                 (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0x080UL))
#define FDCAN_RXF0S                 (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0x090UL))
#define FDCAN_RXF0A                 (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0x094UL))
#define FDCAN_TXBC                  (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0x0C0UL))
#define FDCAN_TXFQS                 (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0x0C4UL))
#define FDCAN_TXBAR                 (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0x0CCUL))
#define FDCAN_CKDIV                 (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0x100UL))

/* CCCR Bits */
#define FDCAN_CCCR_INIT             (1UL << 0)
#define FDCAN_CCCR_CCE              (1UL << 1)
#define FDCAN_CCCR_CSA              (1UL << 3)
#define FDCAN_CCCR_CSR              (1UL << 4)
#define FDCAN_CCCR_FDOE             (1UL << 8)  /* FD Operation Enable */
#define FDCAN_CCCR_BRSE             (1UL << 9)  /* Bit Rate Switch Enable */

/* Fixed Message RAM Offsets on STM32L5/U5 (from stm32l5xx_hal_fdcan.c) */
#define FDCAN_SRAM_RF0SA_OFFSET     0x0B0UL     /* Rx FIFO 0 Start Address (after 28 std + 8 ext filters) */
#define FDCAN_SRAM_TFQSA_OFFSET     0x278UL     /* Tx FIFO/Queue Start Address */
#define FDCAN_SRAM_SIZE_BYTES       0x350UL     /* Total Message RAM size per instance */
#define FDCAN_SRAM_ELEM_BYTES       (18UL * 4UL)/* 72 bytes per 64-byte CAN-FD element */

/* DLC to bytes conversion lookup */
static const uint8_t s_dlc_to_bytes[16] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64
};

static uint8_t BytesToDLC(uint8_t bytes)
{
    if (bytes <= 8)  return bytes;
    if (bytes <= 12) return 9;
    if (bytes <= 16) return 10;
    if (bytes <= 20) return 11;
    if (bytes <= 24) return 12;
    if (bytes <= 32) return 13;
    if (bytes <= 48) return 14;
    return 15; /* 64 bytes */
}

static void FDCAN_ConfigureClocks(void)
{
#if defined(STM32L552xx)
    /* 1. Enable HSI16 (16 MHz) and switch SYSCLK to HSI16 (0 Flash WS in Range 1 up to 20 MHz).
     *    Keeping PCLK1 = 16 MHz also satisfies fdcan_tq_ck <= PCLK1 for a 16 MHz kernel clock. */
    RCC_CR |= (1UL << 8);  /* HSION */
    while ((RCC_CR & (1UL << 10)) == 0U); /* Wait HSIRDY */

    RCC_CFGR = (RCC_CFGR & ~0x3UL) | 0x1UL; /* SW = 01b (HSI16) */
    while ((RCC_CFGR & (0x3UL << 2)) != (0x1UL << 2)); /* Wait SWS == HSI16 */

    /* 2. Ensure PLL is not selected by FDCAN1 and disable PLL before reconfiguring */
    RCC_CCIPR1 &= ~(0x3UL << 24); /* FDCANSEL = 00b */
    RCC_CR &= ~(1UL << 24);       /* PLLON = 0 */
    while ((RCC_CR & (1UL << 25)) != 0U); /* Wait PLLRDY == 0 */

    /* 3. Configure PLL from HSI16 (16 MHz):
     *    PLLSRC = 10b (HSI16), PLLM = 1 (0 -> /1, VCO_in = 16 MHz),
     *    PLLN = 8 (VCO_out = 16 * 8 = 128 MHz), PLLQEN = 1, PLLQ = /8 (11b) -> PLLQ = 16 MHz */
    RCC_PLLCFGR = (2UL << 0)  | /* PLLSRC = HSI16 */
                  (0UL << 4)  | /* PLLM = 1 (/1) */
                  (8UL << 8)  | /* PLLN = 8 (128 MHz VCO) */
                  (1UL << 20) | /* PLLQEN = 1 */
                  (3UL << 21);  /* PLLQ = /8 (16 MHz) */

    RCC_CR |= (1UL << 24); /* PLLON */
    while ((RCC_CR & (1UL << 25)) == 0U); /* Wait PLLRDY */

    /* 4. Select PLL Q (01b) as FDCAN1 kernel clock (16 MHz) */
    RCC_CCIPR1 = (RCC_CCIPR1 & ~(0x3UL << 24)) | (1UL << 24);

    /* 5. Enable GPIOB (AHB2) and FDCAN1 (APB1ENR2 bit 9) peripheral clocks */
    RCC_AHB2ENR  |= (1UL << 1); /* GPIOBEN */
    RCC_APB1ENR2 |= (1UL << 9); /* FDCAN1EN */
#else /* STM32U535xx */
    /* 1. Enable PWR clock and raise VCORE from Range 4 (reset default, where PLLs are forbidden)
     *    to Range 3 (VOS = 01b, supports up to 55 MHz and 0 Flash WS up to 24 MHz). */
    RCC_AHB3ENR |= (1UL << 2); /* PWREN */
    __asm volatile ("nop\n nop");

    if ((PWR_SVMSR & (3UL << 16)) == 0U) {
        PWR_VOSR = (PWR_VOSR & ~((3UL << 16) | (1UL << 18))) | (1UL << 16); /* VOS = Range 3, BOOSTEN = 0 */
        while ((PWR_VOSR & (1UL << 15)) == 0U);  /* Wait VOSRDY */
        while ((PWR_SVMSR & (1UL << 15)) == 0U); /* Wait ACTVOSRDY */
    }

    /* 2. Enable HSI16 (16 MHz) and switch SYSCLK to HSI16 (0 Flash WS in Range 3).
     *    Keeping PCLK1 = 16 MHz satisfies fdcan_tq_ck <= PCLK1 for a 16 MHz kernel clock. */
    RCC_CR |= (1UL << 8); /* HSION */
    while ((RCC_CR & (1UL << 10)) == 0U); /* Wait HSIRDY */

    RCC_CFGR1 = (RCC_CFGR1 & ~0x3UL) | 0x1UL; /* SW = 01b (HSI16) */
    while ((RCC_CFGR1 & (0x3UL << 2)) != (0x1UL << 2)); /* Wait SWS == HSI16 */

    /* 3. Ensure PLL1 is not selected by FDCAN1 and disable PLL1 before reconfiguring */
    RCC_CCIPR1 &= ~(0x3UL << 24); /* FDCAN1SEL = 00b */
    RCC_CR &= ~(1UL << 24);       /* PLL1ON = 0 */
    while ((RCC_CR & (1UL << 25)) != 0U); /* Wait PLL1RDY == 0 */

    /* Clear BOOSTEN temporarily if set before modifying PLL1MBOOST */
    uint32_t boost_was_on = (PWR_VOSR & (1UL << 18));
    if (boost_was_on != 0U) {
        PWR_VOSR &= ~(1UL << 18);
    }

    /* 4. Configure PLL1 from HSI16 (16 MHz):
     *    PLL1SRC = 10b (HSI16), PLL1RGE = 11b (8..16 MHz VCI range),
     *    PLL1M = 0 (/1 -> VCO_in = 16 MHz), PLL1MBOOST = 0 (/1), PLL1QEN = 1,
     *    PLL1N = 11 (N = 12 -> VCO_out = 16 * 12 = 192 MHz),
     *    PLL1Q = 11 (Q = 12 -> PLL1Q = 192 / 12 = 16 MHz).
     *    PLL1P and PLL1R kept at valid reset-equivalent /2 (value 1). */
    RCC_PLL1CFGR = (2UL << 0)  | /* PLL1SRC = HSI16 */
                   (3UL << 2)  | /* PLL1RGE = 8..16 MHz */
                   (0UL << 8)  | /* PLL1M = /1 */
                   (0UL << 12) | /* PLL1MBOOST = /1 */
                   (1UL << 17);  /* PLL1QEN = 1 */

    RCC_PLL1DIVR = (11UL << 0)  | /* PLL1N = 12 (11 + 1 -> 192 MHz VCO) */
                   (1UL  << 9)  | /* PLL1P = /2 */
                   (11UL << 16) | /* PLL1Q = /12 (11 + 1 -> 16 MHz) */
                   (1UL  << 24);  /* PLL1R = /2 */

    if (boost_was_on != 0U) {
        PWR_VOSR |= (1UL << 18);
        while ((PWR_VOSR & (1UL << 14)) == 0U); /* Wait BOOSTRDY */
    }

    RCC_CR |= (1UL << 24); /* PLL1ON */
    while ((RCC_CR & (1UL << 25)) == 0U); /* Wait PLL1RDY */

    /* 5. Select PLL1 Q (01b) as FDCAN1 kernel clock (16 MHz) */
    RCC_CCIPR1 = (RCC_CCIPR1 & ~(0x3UL << 24)) | (1UL << 24);

    /* 6. Enable GPIOB (AHB2ENR1) and FDCAN1 (APB1ENR2 bit 9) peripheral clocks */
    RCC_AHB2ENR1 |= (1UL << 1); /* GPIOBEN */
    RCC_APB1ENR2 |= (1UL << 9); /* FDCAN1EN */
#endif

    __asm volatile ("nop\n nop\n nop\n nop");
}

bool FDCAN_Driver_Init(void)
{
    /* 1. Configure RCC Clocks (HSI16 + PLL -> 16 MHz FDCAN1 kernel clock) & GPIO/FDCAN1 bus clocks */
    FDCAN_ConfigureClocks();

    /* 2. Configure PB8 (RX) and PB9 (TX) to AF9 (FDCAN1) */
    /* MODER: Alternate Function (10b) for pin 8 and pin 9 */
    GPIOB_MODER &= ~((3UL << (8 * 2)) | (3UL << (9 * 2)));
    GPIOB_MODER |=  ((2UL << (8 * 2)) | (2UL << (9 * 2)));

    /* AFRH: Pin 8 -> AF9 (bits [3:0]), Pin 9 -> AF9 (bits [7:4]) */
    GPIOB_AFRH &= ~((0xFUL << 0) | (0xFUL << 4));
    GPIOB_AFRH |=  ((9UL   << 0) | (9UL   << 4));

    /* Very high speed */
    GPIOB_OSPEEDR |= ((3UL << (8 * 2)) | (3UL << (9 * 2)));

    /* No pull */
    GPIOB_PUPDR &= ~((3UL << (8 * 2)) | (3UL << (9 * 2)));

    /* 3. Exit Sleep mode if active and enter Configuration Mode */
    FDCAN_CCCR &= ~FDCAN_CCCR_CSR;
    while ((FDCAN_CCCR & FDCAN_CCCR_CSA) != 0U);

    FDCAN_CCCR |= FDCAN_CCCR_INIT;
    while ((FDCAN_CCCR & FDCAN_CCCR_INIT) == 0U);

    FDCAN_CCCR |= FDCAN_CCCR_CCE;

    /* Ensure kernel clock divider is /1 (fdcan_tq_ck = 16 MHz) */
    FDCAN_CKDIV = 0UL;

    /* Enable CAN-FD with Bit Rate Switching (normal mode, ISO frame format, auto-retransmit on) */
    FDCAN_CCCR = FDCAN_CCCR_INIT | FDCAN_CCCR_CCE | FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE;

    /* 4. Configure Bit Timings for 16 MHz FDCAN kernel clock (t_q = 62.5 ns @ prescaler = 1):
     *
     *    Nominal Bit Rate = 1.000 Mbps:
     *      Prescaler = NBRP + 1 = 0 + 1 = 1  -> f_tq = 16 MHz / 1 = 16 MHz
     *      Sync_Seg  = 1 TQ
     *      TSEG1     = NTSEG1 + 1 = 10 + 1 = 11 TQ
     *      TSEG2     = NTSEG2 + 1 =  3 + 1 =  4 TQ
     *      SJW       = NSJW   + 1 =  3 + 1 =  4 TQ (<= TSEG2)
     *      Total TQ  = 1 + 11 + 4 = 16 TQ  -> 16 MHz / 16 = 1.000 Mbps
     *      Sample Pt = (1 + 11) / 16 = 75.0%
     *
     *    Data Bit Rate = 2.000 Mbps:
     *      Prescaler = DBRP + 1 = 0 + 1 = 1  -> f_tq = 16 MHz / 1 = 16 MHz
     *      Sync_Seg  = 1 TQ
     *      TSEG1     = DTSEG1 + 1 = 4 + 1 = 5 TQ
     *      TSEG2     = DTSEG2 + 1 = 1 + 1 = 2 TQ
     *      SJW       = DSJW   + 1 = 1 + 1 = 2 TQ (<= TSEG2)
     *      Total TQ  = 1 + 5 + 2 = 8 TQ   -> 16 MHz / 8 = 2.000 Mbps
     *      Sample Pt = (1 + 5) / 8 = 75.0%
     */
    FDCAN_NBTP = ((3UL << 25) | (0UL << 16) | (10UL << 8) | 3UL);
    FDCAN_DBTP = ((0UL << 16) | (4UL << 8)  | (1UL  << 4) | 1UL);

    /* 5. Flush FDCAN1 Message RAM (0x000..0x350) */
    for (uint32_t addr = FDCAN1_MSG_RAM_BASE; addr < (FDCAN1_MSG_RAM_BASE + FDCAN_SRAM_SIZE_BYTES); addr += 4UL) {
        *(volatile uint32_t *)addr = 0UL;
    }

    /* 6. Configure Global Filter:
     *    ANFS = 00b (accept non-matching standard frames into RX FIFO 0)
     *    ANFE = 00b (accept non-matching extended frames into RX FIFO 0)
     *    LSS = 0, LSE = 0 (no active filter lists) */
    FDCAN_RXGFC = 0UL;

    /* 7. Configure TX Buffer Mode:
     *    STM32L5/U5 TXBC only defines bit 24 (TFQM: 0 = TX FIFO, 1 = TX Queue). */
    FDCAN_TXBC = 0UL;

    return true;
}

bool FDCAN_Driver_Start(void)
{
    /* Leave Initialization mode to participate in CAN traffic */
    FDCAN_CCCR &= ~FDCAN_CCCR_INIT;
    while ((FDCAN_CCCR & FDCAN_CCCR_INIT) != 0U);

    return true;
}

void FDCAN_Driver_Stop(void)
{
    /* Enter Init mode to cleanly freeze peripheral before jumping to application */
    FDCAN_CCCR |= FDCAN_CCCR_INIT;
    while ((FDCAN_CCCR & FDCAN_CCCR_INIT) == 0U);
}

bool FDCAN_Driver_Receive(FDCAN_Message_t *msg)
{
    if (msg == NULL) return false;

    /* Check if RX FIFO 0 has at least one message */
    uint32_t rxf0s = FDCAN_RXF0S;
    uint32_t fill_level = rxf0s & 0x0FUL;
    if (fill_level == 0U) {
        return false;
    }

    uint32_t get_index = (rxf0s >> 8) & 0x3UL;
    volatile uint32_t *elem = (volatile uint32_t *)(FDCAN1_MSG_RAM_BASE + FDCAN_SRAM_RF0SA_OFFSET +
                                                    (get_index * FDCAN_SRAM_ELEM_BYTES));

    /* Parse Header */
    uint32_t w0 = elem[0];
    uint32_t w1 = elem[1];

    msg->id = (w0 >> 18) & 0x7FFUL; /* Standard 11-bit ID */
    msg->is_fd = ((w1 & (1UL << 21)) != 0U);
    msg->brs = ((w1 & (1UL << 20)) != 0U);

    uint8_t dlc = (uint8_t)((w1 >> 16) & 0x0FUL);
    msg->len = s_dlc_to_bytes[dlc];
    if (!msg->is_fd && msg->len > 8U) {
        msg->len = 8U;
    }

    /* Copy payload words from Message RAM (32-bit APB accesses) */
    uint32_t word_count = ((uint32_t)msg->len + 3UL) / 4UL;
    for (uint32_t w = 0; w < word_count; w++) {
        uint32_t word = elem[2U + w];
        uint32_t base = w * 4UL;
        for (uint32_t b = 0; b < 4UL && (base + b) < msg->len; b++) {
            msg->data[base + b] = (uint8_t)((word >> (b * 8UL)) & 0xFFUL);
        }
    }

    /* Acknowledge read in RX FIFO 0 */
    FDCAN_RXF0A = get_index;

    return true;
}

bool FDCAN_Driver_Transmit(const FDCAN_Message_t *msg)
{
    if (msg == NULL || msg->len > 64U) return false;

    /* Check if TX FIFO/Queue has a free buffer */
    uint32_t tx_fqs = FDCAN_TXFQS;
    if ((tx_fqs & (1UL << 21)) != 0U) {
        return false; /* TX FIFO full */
    }

    uint32_t put_index = (tx_fqs >> 16) & 0x3UL;
    uint32_t buf_offset = FDCAN_SRAM_TFQSA_OFFSET + (put_index * FDCAN_SRAM_ELEM_BYTES);
    volatile uint32_t *elem = (volatile uint32_t *)(FDCAN1_MSG_RAM_BASE + buf_offset);

    /* Construct T0: Standard 11-bit ID */
    elem[0] = ((msg->id & 0x7FFUL) << 18);

    /* Construct T1: DLC, FDF (CAN-FD format), BRS (Bit Rate Switch) */
    uint8_t dlc = BytesToDLC(msg->len);
    uint32_t t1 = ((uint32_t)dlc << 16);
    if (msg->is_fd) {
        t1 |= (1UL << 21); /* FDF */
        if (msg->brs) {
            t1 |= (1UL << 20); /* BRS */
        }
    }
    elem[1] = t1;

    /* Copy payload to Message RAM using 32-bit word writes */
    uint32_t word_count = ((uint32_t)msg->len + 3UL) / 4UL;
    for (uint32_t w = 0; w < word_count; w++) {
        uint32_t word = 0UL;
        uint32_t base = w * 4UL;
        for (uint32_t b = 0; b < 4UL && (base + b) < msg->len; b++) {
            word |= ((uint32_t)msg->data[base + b] << (b * 8UL));
        }
        elem[2U + w] = word;
    }

    /* Request transmission */
    FDCAN_TXBAR = (1UL << put_index);

    /* Wait for transmission completion or timeout */
    uint32_t timeout = 500000;
    while ((FDCAN_TXBAR & (1UL << put_index)) != 0U && --timeout > 0U);

    return (timeout > 0U);
}

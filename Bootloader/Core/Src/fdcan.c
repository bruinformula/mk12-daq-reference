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
#define RCC_AHB2ENR                 (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x4CUL))
#define RCC_APB1ENR2                (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x5CUL))
#else
#define RCC_BASE_ADDR               0x46020C00UL
#define RCC_AHB2ENR1                (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x8CUL))
#define RCC_APB1ENR1                (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x9CUL))
#endif

#define GPIOB_BASE_ADDR             0x42020400UL
#define GPIOB_MODER                 (*(volatile uint32_t *)(GPIOB_BASE_ADDR + 0x00UL))
#define GPIOB_AFRH                  (*(volatile uint32_t *)(GPIOB_BASE_ADDR + 0x24UL))
#define GPIOB_OSPEEDR               (*(volatile uint32_t *)(GPIOB_BASE_ADDR + 0x08UL))
#define GPIOB_PUPDR                 (*(volatile uint32_t *)(GPIOB_BASE_ADDR + 0x0CUL))

#define FDCAN1_BASE_ADDR            0x4000A400UL
#define FDCAN1_MSG_RAM_BASE         0x4000AC00UL

/* FDCAN Registers */
#define FDCAN_CCCR                  (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0x18UL))
#define FDCAN_NBTP                  (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0x1CUL))
#define FDCAN_DBTP                  (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0x0CUL))
#define FDCAN_GFC                   (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0x80UL))
#define FDCAN_SIDFC                 (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0x84UL))
#define FDCAN_RXF0C                 (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0xA0UL))
#define FDCAN_RXF0S                 (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0xA4UL))
#define FDCAN_RXF0A                 (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0xA8UL))
#define FDCAN_TXBC                  (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0xC0UL))
#define FDCAN_TXFQS                 (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0xC4UL))
#define FDCAN_TXBAR                 (*(volatile uint32_t *)(FDCAN1_BASE_ADDR + 0xCCUL))

/* CCCR Bits */
#define FDCAN_CCCR_INIT             (1UL << 0)
#define FDCAN_CCCR_CCE              (1UL << 1)
#define FDCAN_CCCR_FDOE             (1UL << 8)  /* FD Operation Enable */
#define FDCAN_CCCR_BRSE             (1UL << 9)  /* Bit Rate Switch Enable */

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

bool FDCAN_Driver_Init(void)
{
    /* 1. Enable Clocks for GPIOB and FDCAN1 */
#if defined(STM32L552xx)
    RCC_AHB2ENR  |= (1UL << 1);  /* GPIOBEN */
    RCC_APB1ENR2 |= (1UL << 9);  /* FDCAN1EN (bit 9 of APB1ENR2) */
#else
    RCC_AHB2ENR1 |= (1UL << 1);  /* GPIOBEN */
    RCC_APB1ENR1 |= (1UL << 25); /* FDCAN1EN (bit 25 of APB1ENR1) */
#endif

    __asm volatile ("nop\n nop\n nop\n nop");

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

    /* 3. Enter Configuration Mode */
    FDCAN_CCCR |= FDCAN_CCCR_INIT;
    while ((FDCAN_CCCR & FDCAN_CCCR_INIT) == 0);

    FDCAN_CCCR |= FDCAN_CCCR_CCE;

    /* Enable CAN-FD with Bit Rate Switching */
    FDCAN_CCCR |= (FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE);

    /* 4. Configure Bit Timings (Assuming 16 MHz / 110 MHz standard PLL)
     * For 1 Mbps Nominal: Prescaler 1, TimeSeg1 11, TimeSeg2 4 (16 quanta)
     * For 2 Mbps Data:    Prescaler 1, TimeSeg1 5,  TimeSeg2 2 (8 quanta) */
    FDCAN_NBTP = ((0UL << 25) | (3UL << 16) | (10UL << 8) | 3UL);
    FDCAN_DBTP = ((0UL << 16) | (1UL << 4)  | (4UL  << 8) | 1UL);

    /* 5. Configure Global Filter: Accept Non-Matching Standard Frames into RX FIFO 0 */
    FDCAN_GFC = 0; /* Accept non-matching standard & extended frames into RX FIFO 0 */

    /* 6. Configure RX FIFO 0:
     * - Start address: FDCAN1_MSG_RAM_BASE
     * - Size: 3 elements
     * - Watermark: 0 */
    FDCAN_RXF0C = ((3UL << 16) | 0UL);

    /* 7. Configure TX Buffer:
     * - Start address: FDCAN1_MSG_RAM_BASE + 0x100
     * - Dedicated TX buffers: 3
     * - FIFO/Queue: 0 */
    FDCAN_TXBC = ((3UL << 24) | (0x40UL << 2));

    return true;
}

bool FDCAN_Driver_Start(void)
{
    /* Leave Initialization mode to participate in CAN traffic */
    FDCAN_CCCR &= ~FDCAN_CCCR_INIT;
    while ((FDCAN_CCCR & FDCAN_CCCR_INIT) != 0);

    return true;
}

void FDCAN_Driver_Stop(void)
{
    /* Enter Init mode to cleanly freeze peripheral before jumping to application */
    FDCAN_CCCR |= FDCAN_CCCR_INIT;
    while ((FDCAN_CCCR & FDCAN_CCCR_INIT) == 0);
}

bool FDCAN_Driver_Receive(FDCAN_Message_t *msg)
{
    if (msg == NULL) return false;

    /* Check if RX FIFO 0 has at least one message */
    uint32_t rxf0s = FDCAN_RXF0S;
    uint32_t fill_level = rxf0s & 0x7FUL;
    if (fill_level == 0) {
        return false;
    }

    uint32_t get_index = (rxf0s >> 8) & 0x3UL;
    volatile uint32_t *elem = (volatile uint32_t *)(FDCAN1_MSG_RAM_BASE + (get_index * 18 * 4));

    /* Parse Header */
    uint32_t w0 = elem[0];
    uint32_t w1 = elem[1];

    msg->id = (w0 >> 18) & 0x7FFUL; /* Standard 11-bit ID */
    msg->is_fd = ((w1 & (1UL << 21)) != 0);

    uint8_t dlc = (w1 >> 16) & 0x0FUL;
    msg->len = s_dlc_to_bytes[dlc];

    /* Copy payload words into data array */
    volatile uint8_t *payload_bytes = (volatile uint8_t *)&elem[2];
    for (uint8_t i = 0; i < msg->len; i++) {
        msg->data[i] = payload_bytes[i];
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
    if ((tx_fqs & (1UL << 21)) != 0) {
        return false; /* TX FIFO full */
    }

    uint32_t put_index = (tx_fqs >> 16) & 0x1FUL;
    uint32_t buf_offset = 0x100UL + (put_index * 18 * 4);
    volatile uint32_t *elem = (volatile uint32_t *)(FDCAN1_MSG_RAM_BASE + buf_offset);

    /* Construct T0: Standard 11-bit ID */
    elem[0] = ((msg->id & 0x7FFUL) << 18);

    /* Construct T1: DLC, FDF (CAN-FD format), BRS (Bit Rate Switch) */
    uint8_t dlc = BytesToDLC(msg->len);
    uint32_t t1 = ((uint32_t)dlc << 16);
    if (msg->is_fd) {
        t1 |= (1UL << 21); /* FDF */
        t1 |= (1UL << 20); /* BRS */
    }
    elem[1] = t1;

    /* Copy payload */
    volatile uint8_t *payload_bytes = (volatile uint8_t *)&elem[2];
    for (uint8_t i = 0; i < msg->len; i++) {
        payload_bytes[i] = msg->data[i];
    }

    /* Request transmission */
    FDCAN_TXBAR = (1UL << put_index);

    /* Wait for transmission completion or timeout */
    uint32_t timeout = 500000;
    while ((FDCAN_TXBAR & (1UL << put_index)) != 0 && --timeout > 0);

    return (timeout > 0);
}

/**
 * @file    fdcan.c
 * @brief   STM32U5 Bosch M_CAN / FDCAN1 Driver Implementation (PB8 RX, PB9 TX)
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#include "fdcan.h"

/* ========================================================================= */
/* Peripheral & Register Base Addresses                                      */
/* ========================================================================= */

#define RCC_BASE_ADDR               0x46020C00UL
#define RCC_AHB2ENR1                (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x8CUL))
#define RCC_APB1ENR1                (*(volatile uint32_t *)(RCC_BASE_ADDR + 0x9CUL))

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
    RCC_AHB2ENR1 |= (1UL << 1);  /* GPIOBEN */
    RCC_APB1ENR1 |= (1UL << 25); /* FDCAN1EN */

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
    FDCAN_GFC = 0; /* ANFS=0 (Accept in Rx FIFO 0), ANFE=0 */

    /* 6. Configure RX FIFO 0: 3 Elements of 64-byte payload */
    /* FDCAN1_MSG_RAM_BASE: RX FIFO 0 start at offset 0, 3 elements */
    FDCAN_RXF0C = ((18UL << 16) | 3UL); /* F0S = 3 elements, 64-byte data */

    /* 7. Configure TX Buffers: 3 Elements of 64-byte payload */
    FDCAN_TXBC = ((18UL << 16) | (3UL << 24)); /* Dedicated TX FIFO */

    return true;
}

bool FDCAN_Driver_Start(void)
{
    /* Exit initialization mode to connect to bus */
    FDCAN_CCCR &= ~FDCAN_CCCR_INIT;
    while ((FDCAN_CCCR & FDCAN_CCCR_INIT) != 0);

    return true;
}

void FDCAN_Driver_Stop(void)
{
    FDCAN_CCCR |= FDCAN_CCCR_INIT;
    while ((FDCAN_CCCR & FDCAN_CCCR_INIT) == 0);
}

bool FDCAN_Driver_Receive(FDCAN_Message_t *msg)
{
    if (msg == NULL) return false;

    /* Check RX FIFO 0 Fill Level (F0FL bits [6:0]) */
    uint32_t f0s = FDCAN_RXF0S;
    uint32_t fill_level = f0s & 0x7FUL;
    if (fill_level == 0) {
        return false;
    }

    /* Get Get Index (F0GI bits [13:8]) */
    uint32_t get_index = (f0s >> 8) & 0x03UL;

    /* Each 64-byte Rx element in Message RAM is 18 words (72 bytes):
     * Header = 2 words (8 bytes), Data = 16 words (64 bytes) */
    volatile uint32_t *element_addr = (volatile uint32_t *)(FDCAN1_MSG_RAM_BASE + (get_index * 18UL * 4UL));

    uint32_t r0 = element_addr[0];
    uint32_t r1 = element_addr[1];

    /* Extract Standard ID (Bits 28:18) */
    msg->id = (r0 >> 18) & 0x7FFUL;

    /* Extract FD Format & BRS flags */
    msg->is_fd = (r1 & (1UL << 21)) != 0;
    msg->brs   = (r1 & (1UL << 20)) != 0;

    /* Extract DLC */
    uint8_t dlc = (r1 >> 16) & 0x0FUL;
    msg->len = s_dlc_to_bytes[dlc];

    /* Copy payload */
    volatile uint8_t *data_src = (volatile uint8_t *)(&element_addr[2]);
    for (uint8_t i = 0; i < msg->len; i++) {
        msg->data[i] = data_src[i];
    }

    /* Acknowledge message read in RX FIFO 0 */
    FDCAN_RXF0A = get_index;

    return true;
}

bool FDCAN_Driver_Transmit(const FDCAN_Message_t *msg)
{
    if (msg == NULL) return false;

    /* Check TX FIFO Free Level (TFFL bits [2:0]) */
    uint32_t tx_fqs = FDCAN_TXFQS;
    if ((tx_fqs & (1UL << 21)) != 0) {
        /* TX FIFO Full */
        return false;
    }

    /* Get Put Index (TFQPI bits [20:16]) */
    uint32_t put_index = (tx_fqs >> 16) & 0x03UL;

    /* Dedicated Tx Buffer element address (following Rx FIFO 0 in Message RAM) */
    /* Rx FIFO 0 has 3 elements * 18 words = 54 words */
    volatile uint32_t *tx_element = (volatile uint32_t *)(FDCAN1_MSG_RAM_BASE + (54UL * 4UL) + (put_index * 18UL * 4UL));

    /* Build T0 header: Standard ID in bits [28:18] */
    uint32_t t0 = ((msg->id & 0x7FFUL) << 18);

    /* Build T1 header: DLC, FDF, BRS */
    uint8_t dlc = BytesToDLC(msg->len);
    uint32_t t1 = ((uint32_t)dlc << 16);
    if (msg->is_fd) {
        t1 |= (1UL << 21); /* FDF */
        if (msg->brs) {
            t1 |= (1UL << 20); /* BRS */
        }
    }

    tx_element[0] = t0;
    tx_element[1] = t1;

    /* Write data words */
    volatile uint8_t *tx_data_dst = (volatile uint8_t *)(&tx_element[2]);
    for (uint8_t i = 0; i < msg->len; i++) {
        tx_data_dst[i] = msg->data[i];
    }

    /* Request transmission */
    FDCAN_TXBAR = (1UL << put_index);

    return true;
}

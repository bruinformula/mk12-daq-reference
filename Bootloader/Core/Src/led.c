/**
 * @file    led.c
 * @brief   BFR Status LED Driver Implementation (PB7 on Mk12, Safe Stub on Mk11)
 * @author  Bruin Formula Racing (UCLA Formula SAE)
 */

#include "led.h"
#include "main.h"

#if defined(MCU_HAS_STATUS_LED)

/* Peripheral Base Addresses for STM32U5 */
#define RCC_BASE_ADDR           0x46020C00UL
#define RCC_AHB2ENR1_OFFSET     0x0000008CUL
#define RCC_AHB2ENR1            (*(volatile uint32_t *)(RCC_BASE_ADDR + RCC_AHB2ENR1_OFFSET))
#define RCC_GPIOBEN_BIT         (1UL << 1)

#define GPIOB_BASE_ADDR         0x42020400UL
#define GPIO_MODER_OFFSET       0x00UL
#define GPIO_OTYPER_OFFSET      0x04UL
#define GPIO_OSPEEDR_OFFSET     0x08UL
#define GPIO_PUPDR_OFFSET       0x0CUL
#define GPIO_ODR_OFFSET         0x14UL
#define GPIO_BSRR_OFFSET        0x18UL

#define GPIOB_MODER             (*(volatile uint32_t *)(GPIOB_BASE_ADDR + GPIO_MODER_OFFSET))
#define GPIOB_OTYPER            (*(volatile uint32_t *)(GPIOB_BASE_ADDR + GPIO_OTYPER_OFFSET))
#define GPIOB_OSPEEDR           (*(volatile uint32_t *)(GPIOB_BASE_ADDR + GPIO_OSPEEDR_OFFSET))
#define GPIOB_PUPDR             (*(volatile uint32_t *)(GPIOB_BASE_ADDR + GPIO_PUPDR_OFFSET))
#define GPIOB_ODR               (*(volatile uint32_t *)(GPIOB_BASE_ADDR + GPIO_ODR_OFFSET))
#define GPIOB_BSRR              (*(volatile uint32_t *)(GPIOB_BASE_ADDR + GPIO_BSRR_OFFSET))

void LED_Init(void)
{
    /* Enable GPIOB Clock */
    RCC_AHB2ENR1 |= RCC_GPIOBEN_BIT;

    /* Small delay for peripheral clock to stabilize */
    __asm volatile ("nop\n nop\n nop\n nop");

    /* Configure PB7 as Output (MODER bits [15:14] = 01) */
    GPIOB_MODER &= ~(3UL << (7 * 2));
    GPIOB_MODER |=  (1UL << (7 * 2));

    /* Output type: Push-Pull (OTYPER bit 7 = 0) */
    GPIOB_OTYPER &= ~(1UL << 7);

    /* Output speed: Low (OSPEEDR bits [15:14] = 00) */
    GPIOB_OSPEEDR &= ~(3UL << (7 * 2));

    /* No pull-up, no pull-down (PUPDR bits [15:14] = 00) */
    GPIOB_PUPDR &= ~(3UL << (7 * 2));

    /* Start with LED OFF (PB7 LOW) */
    LED_Off();
}

void LED_On(void)
{
    /* PB7 HIGH turns on Q4 -> LED D18 ON */
    GPIOB_BSRR = (1UL << 7);
}

void LED_Off(void)
{
    /* PB7 LOW turns off Q4 -> LED D18 OFF */
    GPIOB_BSRR = (1UL << (7 + 16));
}

void LED_Toggle(void)
{
    GPIOB_ODR ^= (1UL << 7);
}

#else

/* Safe stubs for targets without dedicated PB7 LED (e.g. STM32L552 / Mk11) */
void LED_Init(void) {}
void LED_On(void) {}
void LED_Off(void) {}
void LED_Toggle(void) {}

#endif

#pragma once
#include <stdint.h>

#define UART2_BASE 0xfe201400
#define UART3_BASE 0xfe201600
#define UART4_BASE 0xfe201800
#define UART5_BASE 0xfe201a00

#define GPIO_PADDR 0xfe200000

#define GPIO_PIN_SEL_INPUT 0b000
#define GPIO_PIN_SEL_OUTPUT 0b001
#define GPIO_PIN_SEL_ALT0 0b100
#define GPIO_PIN_SEL_ALT1 0b101
#define GPIO_PIN_SEL_ALT2 0b110
#define GPIO_PIN_SEL_ALT3 0b111
#define GPIO_PIN_SEL_ALT4 0b011
#define GPIO_PIN_SEL_ALT5 0b010

#define GPIO_PULL_NONE 0b00
#define GPIO_PULL_UP 0b01
#define GPIO_PULL_DOWN 0b10

#define GPIO_UART2_TX_PIN 0
#define GPIO_UART2_RX_PIN 1
#define GPIO_UART3_TX_PIN 4
#define GPIO_UART3_RX_PIN 5
#define GPIO_UART4_TX_PIN 8
#define GPIO_UART4_RX_PIN 9
#define GPIO_UART5_TX_PIN 12
#define GPIO_UART5_RX_PIN 13

#define GPIO_UART2_PIN_ALT GPIO_PIN_SEL_ALT4
#define GPIO_UART3_PIN_ALT GPIO_PIN_SEL_ALT4
#define GPIO_UART4_PIN_ALT GPIO_PIN_SEL_ALT4
#define GPIO_UART5_PIN_ALT GPIO_PIN_SEL_ALT4

#define GPIO_UART_TX_PULL GPIO_PULL_NONE
#define GPIO_UART_RX_PULL GPIO_PULL_UP

#define GCS_UART_PADDR UART4_BASE
#define FC_UART_PADDR UART5_BASE
#define GCS_UART_TX_PIN GPIO_UART4_TX_PIN
#define GCS_UART_RX_PIN GPIO_UART4_RX_PIN
#define GCS_UART_PIN_ALT GPIO_UART4_PIN_ALT
#define FC_UART_TX_PIN GPIO_UART5_TX_PIN
#define FC_UART_RX_PIN GPIO_UART5_RX_PIN
#define FC_UART_PIN_ALT GPIO_UART5_PIN_ALT

#define TIMER_ADDR 0xfe003000
#define TIMER_FREQ 1000000

struct pl011_regs {
    uint32_t dr;
    uint32_t rsrecr;
    uint32_t reserved0[4];
    uint32_t fr;
    uint32_t reserved1;
    uint32_t ilpr;
    uint32_t ibrd;
    uint32_t fbrd;
    uint32_t lcrh;
    uint32_t cr;
    uint32_t ifls;
    uint32_t imsc;
    uint32_t ris;
    uint32_t mis;
    uint32_t icr;
    uint32_t dmacr;
    uint32_t reserved2[13];
    uint32_t itcr;
    uint32_t itip;
    uint32_t itop;
    uint32_t tdr;
};

struct bcm2711_gpio_regs {
    uint32_t gpfsel[6];
    uint32_t reserved0;
    uint32_t gpset[2];
    uint32_t reserved1;
    uint32_t gpclr[2];
    uint32_t reserved2;
    uint32_t gplev[2];
    uint32_t reserved3;
    uint32_t gpeds[2];
    uint32_t reserved4;
    uint32_t gpren[2];
    uint32_t reserved5;
    uint32_t gpfen[2];
    uint32_t reserved6;
    uint32_t gphen[2];
    uint32_t reserved7;
    uint32_t gplen[2];
    uint32_t reserved8;
    uint32_t gparen[2];
    uint32_t reserved9;
    uint32_t gpafen[2];     // 0x88
    uint32_t reserved[21];
    uint32_t gppud[4];         // 0xe4
};

struct timer_regs {
    uint32_t cs;
    uint32_t clo;
    uint32_t chi;
    uint32_t c0;
    uint32_t c1;
    uint32_t c2;
    uint32_t c3;
};

static inline void uart_putchar(volatile struct pl011_regs *uart, char c)
{
    while (uart->fr & BIT(5))
        ;
    uart->dr = c;
}
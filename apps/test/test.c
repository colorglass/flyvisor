#include <camkes.h>
#include <camkes/io.h>
#include <string.h>
#include <platsupport/mach/mailbox.h>
#include <platsupport/mach/mailbox_util.h>

#include "global.h"
#include "dma.h"

static volatile struct pl011_regs *gcs_uart;
static volatile struct pl011_regs *fc_uart;
static volatile struct timer_regs *timer_regs;

extern void *uart_reg_translate_paddr(uintptr_t paddr, size_t size);
extern void *gpio_reg_translate_paddr(uintptr_t paddr, size_t size);
extern void *timer_reg_translate_paddr(uintptr_t paddr, size_t size);

static inline uint32_t timer_get_ms()
{
    uint64_t tick = timer_regs->clo | ((uint64_t)timer_regs->chi << 32);
    return (uint32_t)(freq_cycles_and_hz_to_ns(tick, TIMER_FREQ) / NS_IN_MS);
}

static inline void timer_set_timeout(uint32_t ms, uint8_t channel)
{
    if(timer_regs->cs & TIMER_CS_Mx(channel))
        timer_regs->cs = TIMER_CS_Mx(channel);

    uint32_t tick = timer_regs->clo;
    uint32_t timeout = tick + (uint32_t)freq_ns_and_hz_to_cycles(ms * NS_IN_MS, TIMER_FREQ);
    timer_regs->chan[channel] = timeout;
}

static void uart_gpio_configure(int tx_pin, int rx_pin, int alt)
{
    volatile struct bcm2711_gpio_regs *gpio = gpio_reg_translate_paddr(GPIO_PADDR, 0x1000);
    assert(gpio);

    // set pin function
    gpio->gpfsel[tx_pin / 10] &= ~(0b111 << ((tx_pin % 10) * 3));
    gpio->gpfsel[tx_pin / 10] |= alt << ((tx_pin % 10) * 3);
    gpio->gpfsel[rx_pin / 10] &= ~(0b111 << ((rx_pin % 10) * 3));
    gpio->gpfsel[rx_pin / 10] |= alt << ((rx_pin % 10) * 3);

    // set pin pull up/down
    gpio->gppud[tx_pin / 16] &= ~(0b11 << ((tx_pin % 16) * 2));
    gpio->gppud[tx_pin / 16] |= GPIO_UART_TX_PULL << ((tx_pin % 16) * 2);
    gpio->gppud[rx_pin / 16] &= ~(0b11 << ((rx_pin % 16) * 2));
    gpio->gppud[rx_pin / 16] |= GPIO_UART_RX_PULL << ((rx_pin % 16) * 2);
}

static void uart_init(volatile struct pl011_regs *uart, uint32_t baud_rate)
{
    const uint32_t uart_clock = 48000000;
    const uint32_t ibrd = uart_clock / (16 * baud_rate);
    const uint32_t fbrd = (uint32_t)((float)(uart_clock % (16 * baud_rate)) / (float)(16 * baud_rate) * 64 + 0.5);

    // disable uart
    uart->cr &= ~BIT(0);

    // wait for uart to finish transmitting
    while (uart->fr & BIT(3))
        ;

    // disable all irqs
    uart->imsc = 0x0;

    // clear all irq status
    uart->icr = 0x7FF;

    // set line control register
    // 8 bits data, none parity, 1 stop bit, enable fifo
    uart->lcrh = 0x70;

    // set fifo interrupt level: 1/2
    uart->ifls = 0x12;

    // set baud rate
    uart->ibrd = ibrd;
    uart->fbrd = fbrd;

    // enable rx and rx timeout irq
    // uart->imsc |= BIT(4) | BIT(6);

    // enable rx and tx DMA DREQ
    uart->dmacr |= 0b11;

    // enable rx and tx
    uart->cr |= BIT(9) | BIT(8);

    // enable uart
    uart->cr |= BIT(0);
}

struct dma_channel dma_chan;
static uint8_t buffer[1024];
struct dma_uart_config uart_config = {
        .io_bus_addr = 0x7e201800,
        .permap_in = UART4_RX,
        .permap_out = UART4_TX,
    };
void dma_irq_handle()
{
    printf("DMA IRQ\n");
    dma_chan.regs->cs |= DMA_CS_END | DMA_CS_INT;
    int size = dma_transform_read_get_data(&dma_chan, buffer);
    for(int i = 0; i < size; i++)
        printf("%d ",buffer[i]);
    printf("\n");
    timer_set_timeout(100, TIMER_CHAN_1);
    dma_transform_read_uart(&dma_chan, &uart_config);
    dma_irq_acknowledge();
}

void timer_irq_handle()
{
    printf("timeout\n");
    if(timer_regs->cs & TIMER_CS_Mx(TIMER_CHAN_1)) {
        timer_regs->cs = TIMER_CS_Mx(TIMER_CHAN_1);
        int size = dma_transform_read_get_data(&dma_chan, buffer);
        for(int i = 0; i < size; i++)
            printf("%d ",buffer[i]);
        printf("\n");

        timer_set_timeout(5000, TIMER_CHAN_1);
        dma_reset(&dma_chan);
        dma_transform_read_uart(&dma_chan, &uart_config);
    }
    timer_irq_acknowledge();
}


int run()
{
    int status;
    uart_gpio_configure(GCS_UART_TX_PIN, GCS_UART_RX_PIN, GCS_UART_PIN_ALT);
    uart_gpio_configure(FC_UART_TX_PIN, FC_UART_RX_PIN, FC_UART_PIN_ALT);
    gcs_uart = uart_reg_translate_paddr(GCS_UART_PADDR, 0x200);
    fc_uart = uart_reg_translate_paddr(FC_UART_PADDR, 0x200);
    uart_init(gcs_uart, 57600);
    uart_init(fc_uart, 57600);

    timer_regs = timer_reg_translate_paddr(TIMER_ADDR, 0x1000);

    ps_io_ops_t io_ops;
    camkes_io_ops(&io_ops);
    dma_init(&io_ops.dma_manager, &dma_chan, 0);

    dma_transform_read_uart(&dma_chan, &uart_config);

    while(1);
}



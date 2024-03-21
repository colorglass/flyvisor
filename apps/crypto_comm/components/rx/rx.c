#include <camkes.h>
#include <camkes/io.h>
#include <stdint.h>
#include <stdio.h>
#include <utils/util.h>
#include <utils/frequency.h>

#include <crypto/global.h>
#include <crypto/ring_buffer.h>

#include <rpi4-dma.h>

static volatile struct pl011_regs *gcs_uart;
static volatile struct pl011_regs *fc_uart;
static volatile struct timer_regs *timer_regs;

static struct dma_channel dma_channel0;
static struct dma_channel dma_channel1;
struct dma_uart_config gcs_uart_config = {
    .io_bus_addr = 0x7e201800,
    .permap_in = UART4_RX,
    .permap_out = UART4_TX,
};
struct dma_uart_config fc_uart_config = {
    .io_bus_addr = 0x7e201a00,
    .permap_in = UART5_RX,
    .permap_out = UART5_TX,
};

extern void *uart_reg_translate_paddr(uintptr_t paddr, size_t size);
extern void *gpio_reg_translate_paddr(uintptr_t paddr, size_t size);
extern void *timer_reg_translate_paddr(uintptr_t paddr, size_t size);

extern ring_buffer_t *gcs_msg_buf;
extern ring_buffer_t *fc_msg_buf;

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

    // enable uart rx tx dma signals
    uart->dmacr |= 0b11;

    // enable rx and tx
    uart->cr |= BIT(9) | BIT(8);

    // enable uart
    uart->cr |= BIT(0);
}

void uart_irq_handle(void)
{
    if(gcs_uart->mis & (BIT(4) | BIT(6))) {
        gcs_uart->icr = BIT(4) | BIT(6);
        while(!(gcs_uart->fr & BIT(4))) {
            ring_buffer_put(gcs_msg_buf, gcs_uart->dr);
        }
        gcs_ready_emit();
        timer_set_timeout(5 * MS_IN_S, TIMER_CHAN_1);
    }

    if(fc_uart->mis & (BIT(4) | BIT(6))) {
        fc_uart->icr = BIT(4) | BIT(6);
        while(!(fc_uart->fr & BIT(4))) {
            ring_buffer_put(fc_msg_buf, fc_uart->dr);
        }
        fc_ready_emit();
    }

    gcs_uart->icr = gcs_uart->mis;
    fc_uart->icr = fc_uart->mis;
    uart_irq_acknowledge();
}

static uint8_t buf0[32]; 
void dma_irq0_handle()
{
    printf("DMA0 IRQ\n");
    dma_channel0.regs->cs |= DMA_CS_END | DMA_CS_INT;
    int size = dma_transform_read_get_data(&dma_channel0, buf0);
    for(int i = 0; i < size; i++) {
        ring_buffer_put(gcs_msg_buf, buf0[i]);
    }
    gcs_ready_emit();
    timer_set_timeout(5 * MS_IN_S, TIMER_CHAN_1);

    // DMA data watchdog
    timer_set_timeout(100, TIMER_CHAN_3);
    dma_transform_read_uart(&dma_channel0, &gcs_uart_config);
    dma_irq0_acknowledge();
}

static uint8_t buf1[32];
void dma_irq1_handle()
{
    printf("DMA1 IRQ\n");
    dma_channel1.regs->cs |= DMA_CS_END | DMA_CS_INT;
    int size = dma_transform_read_get_data(&dma_channel1, buf1);
    for(int i = 0; i < size; i++) {
        ring_buffer_put(fc_msg_buf, buf1[i]);
    }
    fc_ready_emit();
    dma_transform_read_uart(&dma_channel1, &fc_uart_config);
    dma_irq1_acknowledge();
}

void timer_irq_handle()
{
    // Data timeout for 
    if(timer_regs->cs & TIMER_CS_Mx(TIMER_CHAN_1)) {
        timer_regs->cs = TIMER_CS_Mx(TIMER_CHAN_1);
        data_timeout_emit();
        timer_set_timeout(1 * MS_IN_MINUTE, TIMER_CHAN_1);
        printf("Data timeout\n");
    }

    if(timer_regs->cs & TIMER_CS_Mx(TIMER_CHAN_3)) {
        timer_regs->cs = TIMER_CS_Mx(TIMER_CHAN_3);
        
    }

    timer_irq_acknowledge();
}

int run()
{
    ring_buffer_init(gcs_msg_buf, MSG_BUFFER_SIZE);
    ring_buffer_init(fc_msg_buf, MSG_BUFFER_SIZE);

    uart_gpio_configure(GCS_UART_TX_PIN, GCS_UART_RX_PIN, GCS_UART_PIN_ALT);
    uart_gpio_configure(FC_UART_TX_PIN, FC_UART_RX_PIN, FC_UART_PIN_ALT);
    gcs_uart = uart_reg_translate_paddr(GCS_UART_PADDR, 0x200);
    fc_uart = uart_reg_translate_paddr(FC_UART_PADDR, 0x200);
    uart_init(gcs_uart, 57600);
    uart_init(fc_uart, 57600);

    timer_regs = timer_reg_translate_paddr(TIMER_ADDR, 0x1000);

    ps_io_ops_t io_ops;
    camkes_io_ops(&io_ops);
    dma_init(&io_ops.dma_manager, &dma_channel0, 0);
    dma_init(&io_ops.dma_manager, &dma_channel1, 1);

    dma_transform_read_uart(&dma_channel0, &gcs_uart_config);
    dma_transform_read_uart(&dma_channel1, &fc_uart_config);

    return 0;
}
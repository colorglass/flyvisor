#pragma once
#include <platsupport/io.h>
#include <stdint.h>

#define BUS_ADDR(addr) ((addr & ~0xc0000000) | 0xc0000000)
#define DMA_CB_ALIGN 32

#define DMA_CS_RESET (1 << 31)
#define DMA_CS_ABORT (1 << 30)
#define DMA_CS_DISDEBUG (1 << 29)
#define DMA_CS_WFOW (1 << 28)
#define DMA_CS_PANIC_PRIORITY(x) ((x & 0xf) << 20)
#define DMA_CS_PRIORITY(x) ((x & 0xf) << 16)
#define DMA_CS_ERROR_STA (1 << 8)
#define DMA_CS_WFOW_STA (1 << 6)
#define DMA_CS_DREQ_STOP_STA (1 << 5)
#define DMA_CS_PAUSED_STA (1 << 4)
#define DMA_CS_DREQ_STA (1 << 3)
#define DMA_CS_INT (1 << 2)
#define DMA_CS_END (1 << 1)
#define DMA_CS_ACTIVE (1 << 0)

#define DMA_TI_NO_WIDE_BURSTS (1 << 26)
#define DMA_TI_WAITS(x) ((x & 0x1f) << 21)
#define DMA_TI_PERMAP(x) ((x & 0x1f) << 16)
#define DMA_TI_BURST_LENGTH(x) ((x & 0xf) << 12)
#define DMA_TI_SRC_IGNORE (1 << 11)
#define DMA_TI_SRC_DREQ (1 << 10)
#define DMA_TI_SRC_WIDTH (1 << 9)
#define DMA_TI_SRC_INC (1 << 8)
#define DMA_TI_DEST_IGNORE (1 << 7)
#define DMA_TI_DEST_DREQ (1 << 6)
#define DMA_TI_DEST_WIDTH (1 << 5)
#define DMA_TI_DEST_INC (1 << 4)
#define DMA_TI_WAIT_RESP (1 << 3)
#define DMA_TI_TDMODE (1 << 1)
#define DMA_TI_INTEN (1 << 0)

enum dma_permap {
    DREQ = 0,
    DSI0 = 1,
    PWM1 = 1,
    PCM_TX = 2,
    PCM_RX = 3,
    SMI = 4,
    PWM0 = 5,
    SPI0_TX = 6,
    SPO0_RX = 7,
    SPI_SLAVE_TX = 8,
    SPI_SLAVE_RX = 9,
    HDMI0 = 10,
    eMMC = 11,
    UART0_TX = 12,
    SD_HOST = 13,
    UART0_RX = 14,
    DSI1 = 15,
    SPI1_TX = 16,
    HDMI1 = 17,
    SPI1_RX = 18,
    UART3_TX = 19,
    SPI4_TX = 19,
    UART3_RX = 20,
    SPI4_RX = 20,
    UART5_TX = 21,
    SPI5_TX = 21,
    UART5_RX = 22,
    SPI5_RX = 22,
    SPI6_TX = 23,
    FIFO0 = 24,
    FIFO1 = 25,
    FIFO2 = 26,
    SPI6_RX = 27,
    UART2_TX = 28,
    UART2_RX = 29,
    UART4_TX = 30,
    UART4_RX = 31,
};

struct dma_uart_config{
    intptr_t io_bus_addr;
    int permap_in;
    int permap_out;
};

struct dma_control_block {
    uint32_t ti;
    uint32_t source_ad;
    uint32_t dest_ad;
    uint32_t txfr_len;
    uint32_t stride;
    uint32_t nextconbk;
    uint32_t reserved1;
    uint32_t reserved2;
}; 

struct dma_cb_list {
    struct dma_control_block* cbs;
    size_t entries;
    uintptr_t bus_addr;
    uintptr_t raw_addr;
};

struct dma_channel_regs {
    uint32_t cs;
    uint32_t conblk_addr;
    uint32_t ti;
    uint32_t source_ad;
    uint32_t dest_ad;
    uint32_t txfr_len;
    uint32_t stride;
    uint32_t nextconbk;
    uint32_t debug;
};

struct dma_buffer {
    void* ptr;
    size_t size;
    uintptr_t raw_addr;
    uintptr_t bus_addr;
};

struct dma_channel {
    int channel;
    volatile struct dma_channel_regs* regs;
    struct dma_cb_list cb_list;
    struct dma_buffer buffer;
};

int dma_init(ps_dma_man_t* dma_ops, struct dma_channel* dma_channel, int channel);
int dma_transform_send_uart(struct dma_channel* channel, void* src, size_t size, struct dma_uart_config* uart);
int dma_transform_read_uart(struct dma_channel* channel, struct dma_uart_config* uart);
int dma_transform_read_get_data(struct dma_channel* channel, void* dest);

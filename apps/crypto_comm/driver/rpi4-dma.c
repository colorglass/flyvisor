#include <platsupport/io.h>
#include <string.h>
#include "rpi4-dma.h"

extern volatile void *dma_reg;

static int dma_reset(struct dma_channel* channel)
{
    volatile struct dma_channel_regs* regs = channel->regs;
    regs->cs |= DMA_CS_RESET;
    while(regs->cs & DMA_CS_RESET);
    if(regs->cs & DMA_CS_ERROR_STA) {
        printf("DMA channel %d reset failed\n", channel);
        return -1;
    }

    regs->cs |= DMA_CS_PRIORITY(8) | DMA_CS_PANIC_PRIORITY(8) | DMA_CS_DISDEBUG;
    return 0;
}

int dma_init(ps_dma_man_t* dma_ops, struct dma_channel* dma_channel, int channel)
{
    assert(channel < 15);
    assert(dma_ops);
    assert(dma_channel);

    dma_channel->channel = channel;
    dma_channel->regs = (struct dma_channel_regs*)(dma_reg + 0x100 * channel);
    
    // cb should be aligned to 32 bytes
    dma_channel->buffer.ptr = ps_dma_alloc(dma_ops, PAGE_SIZE_4K, PAGE_SIZE_4K, false, PS_MEM_NORMAL);
    assert(dma_channel->buffer.ptr);
    dma_channel->buffer.raw_addr = ps_dma_pin(dma_ops, dma_channel->buffer.ptr, PAGE_SIZE_4K);
    assert(dma_channel->buffer.raw_addr);
    dma_channel->buffer.bus_addr = BUS_ADDR(dma_channel->buffer.raw_addr);
    dma_channel->buffer.size = PAGE_SIZE_4K;

    memset(dma_channel->buffer.ptr, 0, PAGE_SIZE_4K);

    dma_channel->cb_list.cbs = ps_dma_alloc(dma_ops, PAGE_SIZE_4K, PAGE_SIZE_4K, false, PS_MEM_NORMAL);
    assert(dma_channel->cb_list.cbs);
    dma_channel->cb_list.raw_addr = ps_dma_pin(dma_ops, dma_channel->cb_list.cbs, PAGE_SIZE_4K);
    assert(dma_channel->cb_list.raw_addr);
    dma_channel->cb_list.bus_addr = BUS_ADDR(dma_channel->cb_list.raw_addr);
    dma_channel->cb_list.entries = PAGE_SIZE_4K / sizeof(struct dma_control_block);

    memset(dma_channel->cb_list.cbs, 0, PAGE_SIZE_4K);

    *(volatile uint32_t*)(dma_reg + 0xff0) |= 1 << channel;

    return dma_reset(dma_channel);
}

int dma_transform_send_uart(struct dma_channel* channel, void* src, size_t size, struct dma_uart_config* uart) {

    printf("buffer size: %d, size: %d\n", channel->buffer.size, size);
    if(size * 4 > channel->buffer.size) {
        printf("DMA buffer size too small\n");
        return -1;
    }

    uint32_t* buffer = channel->buffer.ptr;
    for(int i = 0; i < size; i++) {
        buffer[i] = (uint32_t)(((uint8_t*)src)[i] & 0xff);
    }

    if(channel->regs->cs & DMA_CS_ERROR_STA) {
        int err;
        if(err = dma_reset(channel))
            return err;
    }

    struct dma_control_block* cb = &channel->cb_list.cbs[0];
    cb->ti = DMA_TI_SRC_INC | DMA_TI_DEST_DREQ | DMA_TI_PERMAP(uart->permap_out) | DMA_TI_WAIT_RESP;
    cb->source_ad = channel->buffer.bus_addr;
    cb->dest_ad = uart->io_bus_addr;
    cb->txfr_len = size * 4;
    cb->stride = 0;
    cb->nextconbk = 0;

    channel->regs->conblk_addr = channel->cb_list.bus_addr;
    channel->regs->cs |= DMA_CS_ACTIVE;

    return 0;
}
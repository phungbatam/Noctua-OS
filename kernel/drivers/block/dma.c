#include "dma.h"
#include "ports.h"

static const uint8_t dma_addr_regs[] = { 0x00, 0x02, 0x04, 0x06 };
static const uint8_t dma_cnt_regs[] = { 0x01, 0x03, 0x05, 0x07 };
static const uint8_t dma_page_regs[] = { 0x87, 0x83, 0x81, 0x82 };

void dma_init(void) {
    outb(DMA_RESET, 0xFF);
    outb(DMA_MASK_ALL, 0xFF);
}

void dma_mask_channel(uint8_t channel) {
    if (channel > 3) return;
    outb(DMA_MASK_REG, (1 << channel) | (channel & 3));
}

void dma_unmask_channel(uint8_t channel) {
    if (channel > 3) return;
    outb(DMA_MASK_REG, channel & 3);
}

void dma_set_mode(uint8_t channel, uint8_t mode) {
    if (channel > 3) return;
    outb(DMA_MODE_REG, mode | (channel & 3));
}

void dma_set_address(uint8_t channel, uint32_t phys_addr) {
    if (channel > 3) return;
    uint16_t addr = phys_addr & 0xFFFF;
    uint8_t page = (phys_addr >> 16) & 0xFF;

    outb(DMA_CLR_FF, 0xFF);
    outb(dma_addr_regs[channel], addr & 0xFF);
    outb(dma_addr_regs[channel], (addr >> 8) & 0xFF);
    outb(dma_page_regs[channel], page);
}

void dma_set_count(uint8_t channel, uint16_t count) {
    if (channel > 3) return;
    outb(DMA_CLR_FF, 0xFF);
    outb(dma_cnt_regs[channel], (count - 1) & 0xFF);
    outb(dma_cnt_regs[channel], ((count - 1) >> 8) & 0xFF);
}

void dma_prepare_floppy(uint8_t *buffer, uint16_t sector_count) {
    uint32_t phys_addr = (uint32_t)buffer;
    uint16_t count = sector_count * 512;

    dma_mask_channel(2);
    dma_set_mode(2, DMA_MODE_SINGLE | DMA_MODE_READ | DMA_MODE_CH2);
    dma_set_address(2, phys_addr);
    dma_set_count(2, count);
    dma_unmask_channel(2);
}

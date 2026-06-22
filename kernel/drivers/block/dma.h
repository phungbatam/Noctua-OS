#ifndef TVN_DMA_H
#define TVN_DMA_H

#include <stdint.h>

#define DMA_CHANNEL0   0x00
#define DMA_CHANNEL1   0x01
#define DMA_CHANNEL2   0x02
#define DMA_CHANNEL3   0x03

#define DMA_ADDR_CH0   0x00
#define DMA_ADDR_CH1   0x02
#define DMA_ADDR_CH2   0x04
#define DMA_ADDR_CH3   0x06

#define DMA_CNT_CH0    0x01
#define DMA_CNT_CH1    0x03
#define DMA_CNT_CH2    0x05
#define DMA_CNT_CH3    0x07

#define DMA_MASK_REG   0x0A
#define DMA_MODE_REG   0x0B
#define DMA_CLR_FF     0x0C
#define DMA_RESET      0x0D
#define DMA_MASK_ALL   0x0F

#define DMA_PAGE_CH0   0x87
#define DMA_PAGE_CH1   0x83
#define DMA_PAGE_CH2   0x81
#define DMA_PAGE_CH3   0x82

#define DMA_MODE_DEMAND   0x00
#define DMA_MODE_SINGLE   0x40
#define DMA_MODE_BLOCK    0x80
#define DMA_MODE_CASCADE  0xC0

#define DMA_MODE_VERIFY   0x00
#define DMA_MODE_WRITE    0x04
#define DMA_MODE_READ     0x08

#define DMA_MODE_CH0      0x00
#define DMA_MODE_CH1      0x01
#define DMA_MODE_CH2      0x02
#define DMA_MODE_CH3      0x03

void dma_init(void);
void dma_mask_channel(uint8_t channel);
void dma_unmask_channel(uint8_t channel);
void dma_set_mode(uint8_t channel, uint8_t mode);
void dma_set_address(uint8_t channel, uint32_t phys_addr);
void dma_set_count(uint8_t channel, uint16_t count);
void dma_prepare_floppy(uint8_t *buffer, uint16_t sector_count);

#endif

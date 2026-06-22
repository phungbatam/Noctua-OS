#ifndef TVN_ATA_H
#define TVN_ATA_H

#include <stdint.h>

#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376

#define ATA_REG_DATA        0
#define ATA_REG_ERROR       1
#define ATA_REG_SECCOUNT    2
#define ATA_REG_LBA0        3
#define ATA_REG_LBA1        4
#define ATA_REG_LBA2        5
#define ATA_REG_DRIVE       6
#define ATA_REG_CMD         7
#define ATA_REG_STATUS      7

#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_READ_PIO_EXT    0x24
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_WRITE_PIO_EXT   0x34
#define ATA_CMD_IDENTIFY        0xEC
#define ATA_CMD_IDENTIFY_PACKET 0xA1
#define ATA_CMD_FLUSH_CACHE     0xE7
#define ATA_CMD_FLUSH_CACHE_EXT 0xEA
#define ATA_CMD_READ_DMA        0xC8
#define ATA_CMD_READ_DMA_EXT    0x25
#define ATA_CMD_WRITE_DMA       0xCA
#define ATA_CMD_WRITE_DMA_EXT   0x35
#define ATA_CMD_SET_FEATURES    0xEF

#define ATA_STATUS_BSY      0x80
#define ATA_STATUS_DRDY     0x40
#define ATA_STATUS_DRQ      0x08
#define ATA_STATUS_ERR      0x01
#define ATA_STATUS_DF       0x20

#define ATA_DEV_ATA     0x00
#define ATA_DEV_ATAPI   0x01

typedef struct {
    int present;
    int is_master;
    uint16_t io_base;
    uint16_t ctrl_base;
    char model[41];
    char serial[21];
    char firmware[9];
    uint64_t total_sectors;
    uint32_t sector_size;
    uint16_t physical_sector_size;
    int is_atapi;
    int supports_lba48;
    int supports_dma;
    int supports_lba;
    uint16_t pio_mode;
    uint16_t dma_mode;
    uint8_t  major_version;
    uint8_t  minor_version;
} ata_device_t;

#define MAX_ATA_DEVICES 8

int ata_init(void);
int ata_read_sectors(ata_device_t *dev, uint64_t lba, uint16_t count, void *buffer);
int ata_write_sectors(ata_device_t *dev, uint64_t lba, uint16_t count, const void *buffer);
ata_device_t *ata_get_device(int index);
int ata_device_count(void);

#endif

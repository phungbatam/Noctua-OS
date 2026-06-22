#include "ata.h"
#include "ports.h"
#include "string.h"
#include "heap.h"

static ata_device_t ata_devices[MAX_ATA_DEVICES];
static int ata_dev_count = 0;

static void ata_wait_bsy(uint16_t io_base) {
    for (int i = 0; i < 100000; i++) {
        if (!(inb(io_base + ATA_REG_STATUS) & ATA_STATUS_BSY)) return;
    }
}

static void ata_wait_drq(uint16_t io_base) {
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);
        if (status & ATA_STATUS_DRQ) return;
        if (status & ATA_STATUS_ERR) return;
    }
}

static int ata_identify(ata_device_t *dev) {
    uint16_t io = dev->io_base;

    ata_wait_bsy(io);

    outb(io + ATA_REG_DRIVE, dev->is_master ? 0xA0 : 0xB0);
    ata_wait_bsy(io);

    outb(io + ATA_REG_SECCOUNT, 0);
    outb(io + ATA_REG_LBA0, 0);
    outb(io + ATA_REG_LBA1, 0);
    outb(io + ATA_REG_LBA2, 0);
    outb(io + ATA_REG_CMD, ATA_CMD_IDENTIFY);

    uint8_t status = inb(io + ATA_REG_STATUS);
    if (status == 0) return 0;

    for (int i = 0; i < 100000; i++) {
        status = inb(io + ATA_REG_STATUS);
        if (status & ATA_STATUS_ERR) return 0;
        if (status & ATA_STATUS_DRQ) break;
    }
    if (!(status & ATA_STATUS_DRQ)) return 0;

    uint16_t buf[256];
    for (int i = 0; i < 256; i++) {
        buf[i] = inw(io + ATA_REG_DATA);
    }

    for (int i = 0; i < 40; i += 2) {
        dev->model[i] = (buf[27 + i / 2] >> 8) & 0xFF;
        dev->model[i + 1] = buf[27 + i / 2] & 0xFF;
    }
    dev->model[40] = 0;

    for (int i = 39; i >= 0; i--) {
        if (dev->model[i] == ' ') dev->model[i] = 0;
        else break;
    }

    for (int i = 0; i < 20; i += 2) {
        dev->serial[i] = (buf[10 + i / 2] >> 8) & 0xFF;
        dev->serial[i + 1] = buf[10 + i / 2] & 0xFF;
    }
    dev->serial[20] = 0;

    for (int i = 0; i < 8; i += 2) {
        dev->firmware[i] = (buf[23 + i / 2] >> 8) & 0xFF;
        dev->firmware[i + 1] = buf[23 + i / 2] & 0xFF;
    }
    dev->firmware[8] = 0;

    for (int i = 19; i >= 0; i--) {
        if (dev->serial[i] == ' ') dev->serial[i] = 0;
        else break;
    }
    for (int i = 7; i >= 0; i--) {
        if (dev->firmware[i] == ' ') dev->firmware[i] = 0;
        else break;
    }

    uint16_t capabilities = buf[49];
    dev->supports_lba = (capabilities & (1 << 9)) ? 1 : 0;

    dev->supports_dma = (capabilities & (1 << 8)) ? 1 : 0;

    dev->sector_size = 512;
    dev->physical_sector_size = 512;

    if (buf[106] & (1 << 14)) {
        dev->physical_sector_size = (buf[106] & (1 << 12)) ? 4096 : 512;
    }

    dev->is_atapi = 0;
    if (buf[0] & 0x8000) {
        dev->is_atapi = 1;
    }

    dev->supports_lba48 = 0;
    if (buf[83] & (1 << 10)) {
        dev->supports_lba48 = 1;
    }

    if (dev->supports_lba48) {
        dev->total_sectors = *(uint64_t *)&buf[100];
    } else if (dev->supports_lba) {
        dev->total_sectors = *(uint32_t *)&buf[60];
    } else {
        uint32_t cyl = buf[1];
        uint32_t heads = buf[3];
        uint32_t spt = buf[6];
        dev->total_sectors = cyl * heads * spt;
    }

    uint8_t major = (buf[80] >> 8) & 0xFF;
    dev->major_version = major;
    dev->minor_version = buf[80] & 0xFF;

    uint16_t pio = buf[64];
    dev->pio_mode = 4;
    if (pio & 0x02) dev->pio_mode = 3;
    if (pio & 0x01) dev->pio_mode = 0;
    if (pio & 0x04) dev->pio_mode = 4;

    dev->dma_mode = 0;
    uint16_t dma_mword = buf[63];
    if (dma_mword & 0x07) dev->dma_mode = 0;
    if (dma_mword & 0x38) dev->dma_mode = 1;
    if (dma_mword & 0xC0) dev->dma_mode = 2;

    dev->present = 1;
    return 1;
}

static void ata_add_device(int is_master, uint16_t io_base, uint16_t ctrl_base) {
    if (ata_dev_count >= MAX_ATA_DEVICES) return;

    ata_device_t *dev = &ata_devices[ata_dev_count];
    memset(dev, 0, sizeof(ata_device_t));
    dev->is_master = is_master;
    dev->io_base = io_base;
    dev->ctrl_base = ctrl_base;

    if (ata_identify(dev)) {
        ata_dev_count++;
    }
}

int ata_init(void) {
    memset(ata_devices, 0, sizeof(ata_devices));
    ata_dev_count = 0;

    ata_add_device(1, ATA_PRIMARY_IO, ATA_PRIMARY_CTRL);
    ata_add_device(0, ATA_PRIMARY_IO, ATA_PRIMARY_CTRL);
    ata_add_device(1, ATA_SECONDARY_IO, ATA_SECONDARY_CTRL);
    ata_add_device(0, ATA_SECONDARY_IO, ATA_SECONDARY_CTRL);

    return ata_dev_count;
}

int ata_read_sectors(ata_device_t *dev, uint64_t lba, uint16_t count, void *buffer) {
    if (!dev || !dev->present || !buffer) return -1;
    if (count == 0) return 0;

    uint16_t io = dev->io_base;
    uint16_t *buf = (uint16_t *)buffer;

    if (dev->supports_lba48 && lba > 0x0FFFFFFF) {
        ata_wait_bsy(io);

        outb(io + ATA_REG_DRIVE, dev->is_master ? 0x40 : 0x50);
        ata_wait_bsy(io);

        outb(io + ATA_REG_SECCOUNT, (count >> 8) & 0xFF);
        outb(io + ATA_REG_LBA0, (lba >> 24) & 0xFF);
        outb(io + ATA_REG_LBA1, (lba >> 32) & 0xFF);
        outb(io + ATA_REG_LBA2, (lba >> 40) & 0xFF);
        outb(io + ATA_REG_SECCOUNT, count & 0xFF);
        outb(io + ATA_REG_LBA0, lba & 0xFF);
        outb(io + ATA_REG_LBA1, (lba >> 8) & 0xFF);
        outb(io + ATA_REG_LBA2, (lba >> 16) & 0xFF);
        outb(io + ATA_REG_CMD, ATA_CMD_READ_PIO_EXT);

        for (uint16_t s = 0; s < count; s++) {
            ata_wait_drq(io);
            for (int i = 0; i < 256; i++) {
                buf[s * 256 + i] = inw(io + ATA_REG_DATA);
            }
        }
    } else {
        ata_wait_bsy(io);

        outb(io + ATA_REG_DRIVE, dev->is_master ? 0xE0 : 0xF0);
        ata_wait_bsy(io);

        outb(io + ATA_REG_SECCOUNT, count & 0xFF);
        outb(io + ATA_REG_LBA0, lba & 0xFF);
        outb(io + ATA_REG_LBA1, (lba >> 8) & 0xFF);
        outb(io + ATA_REG_LBA2, (lba >> 16) & 0xFF);
        outb(io + ATA_REG_CMD, ATA_CMD_READ_PIO);

        for (uint16_t s = 0; s < count; s++) {
            ata_wait_drq(io);
            for (int i = 0; i < 256; i++) {
                buf[s * 256 + i] = inw(io + ATA_REG_DATA);
            }
        }
    }

    return 0;
}

int ata_write_sectors(ata_device_t *dev, uint64_t lba, uint16_t count, const void *buffer) {
    if (!dev || !dev->present || !buffer) return -1;
    if (count == 0) return 0;

    uint16_t io = dev->io_base;
    uint16_t *buf = (uint16_t *)buffer;

    if (dev->supports_lba48 && lba > 0x0FFFFFFF) {
        ata_wait_bsy(io);

        outb(io + ATA_REG_DRIVE, dev->is_master ? 0x40 : 0x50);
        ata_wait_bsy(io);

        outb(io + ATA_REG_SECCOUNT, (count >> 8) & 0xFF);
        outb(io + ATA_REG_LBA0, (lba >> 24) & 0xFF);
        outb(io + ATA_REG_LBA1, (lba >> 32) & 0xFF);
        outb(io + ATA_REG_LBA2, (lba >> 40) & 0xFF);
        outb(io + ATA_REG_SECCOUNT, count & 0xFF);
        outb(io + ATA_REG_LBA0, lba & 0xFF);
        outb(io + ATA_REG_LBA1, (lba >> 8) & 0xFF);
        outb(io + ATA_REG_LBA2, (lba >> 16) & 0xFF);
        outb(io + ATA_REG_CMD, ATA_CMD_WRITE_PIO_EXT);

        for (uint16_t s = 0; s < count; s++) {
            ata_wait_drq(io);
            for (int i = 0; i < 256; i++) {
                outw(io + ATA_REG_DATA, buf[s * 256 + i]);
            }
        }

        outb(io + ATA_REG_CMD, ATA_CMD_FLUSH_CACHE_EXT);
    } else {
        ata_wait_bsy(io);

        outb(io + ATA_REG_DRIVE, dev->is_master ? 0xE0 : 0xF0);
        ata_wait_bsy(io);

        outb(io + ATA_REG_SECCOUNT, count & 0xFF);
        outb(io + ATA_REG_LBA0, lba & 0xFF);
        outb(io + ATA_REG_LBA1, (lba >> 8) & 0xFF);
        outb(io + ATA_REG_LBA2, (lba >> 16) & 0xFF);
        outb(io + ATA_REG_CMD, ATA_CMD_WRITE_PIO);

        for (uint16_t s = 0; s < count; s++) {
            ata_wait_drq(io);
            for (int i = 0; i < 256; i++) {
                outw(io + ATA_REG_DATA, buf[s * 256 + i]);
            }
        }

        outb(io + ATA_REG_CMD, ATA_CMD_FLUSH_CACHE);
    }

    return 0;
}

ata_device_t *ata_get_device(int index) {
    if (index < 0 || index >= ata_dev_count) return 0;
    return &ata_devices[index];
}

int ata_device_count(void) { return ata_dev_count; }



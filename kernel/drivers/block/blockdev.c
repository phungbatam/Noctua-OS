#include "blockdev.h"
#include "ata.h"
#include "string.h"

#define MAX_BLOCK_DEVS 16

typedef struct {
    block_dev_t dev;
    int ata_index;
    uint64_t lba_start;
    int used;
} blockdev_entry_t;

static blockdev_entry_t entries[MAX_BLOCK_DEVS];
static int entry_count = 0;

static int ata_part_read(void *priv, uint64_t sector, void *buffer, uint32_t count) {
    blockdev_entry_t *e = (blockdev_entry_t *)priv;
    ata_device_t *ata = ata_get_device(e->ata_index);
    if (!ata) return -1;
    return ata_read_sectors(ata, e->lba_start + sector, count, buffer);
}

static int ata_part_write(void *priv, uint64_t sector, const void *buffer, uint32_t count) {
    blockdev_entry_t *e = (blockdev_entry_t *)priv;
    ata_device_t *ata = ata_get_device(e->ata_index);
    if (!ata) return -1;
    return ata_write_sectors(ata, e->lba_start + sector, count, buffer);
}

int blockdev_register(int ata_index, uint64_t lba_start, uint64_t total_sectors) {
    if (entry_count >= MAX_BLOCK_DEVS) return -1;
    blockdev_entry_t *e = &entries[entry_count];
    e->used = 1;
    e->ata_index = ata_index;
    e->lba_start = lba_start;
    e->dev.priv = e;
    e->dev.read = ata_part_read;
    e->dev.write = ata_part_write;
    e->dev.total_sectors = total_sectors;
    e->dev.sector_size = 512;
    entry_count++;
    return entry_count - 1;
}

block_dev_t *blockdev_get(int index) {
    if (index < 0 || index >= entry_count || !entries[index].used) return 0;
    return &entries[index].dev;
}

int blockdev_count(void) {
    return entry_count;
}

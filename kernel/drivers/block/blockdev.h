#ifndef TVN_BLOCKDEV_H
#define TVN_BLOCKDEV_H

#include <stdint.h>

typedef struct block_dev {
    void *priv;
    int (*read)(void *priv, uint64_t sector, void *buffer, uint32_t count);
    int (*write)(void *priv, uint64_t sector, const void *buffer, uint32_t count);
    uint64_t total_sectors;
    uint32_t sector_size;
} block_dev_t;

block_dev_t *blockdev_get(int index);
int blockdev_count(void);
int blockdev_register(int ata_index, uint64_t lba_start, uint64_t total_sectors);

#endif

#ifndef NOCTUA_BLOCKCACHE_H
#define NOCTUA_BLOCKCACHE_H

#include <stdint.h>

#define BCACHE_SIZE       512
#define BCACHE_BLOCK_SIZE 512

typedef struct {
    int      used;
    int      dev_id;
    uint32_t block;
    uint8_t  data[BCACHE_BLOCK_SIZE];
    int      dirty;
    int      recent;
} bcache_block_t;

void bcache_init(void);
uint8_t *bcache_read(int dev_id, uint32_t block);
int      bcache_write(int dev_id, uint32_t block, const uint8_t *data);
void     bcache_flush(void);
void     bcache_flush_dev(int dev_id);
int      bcache_get_stats(int *used, int *dirty, int *total);

#endif

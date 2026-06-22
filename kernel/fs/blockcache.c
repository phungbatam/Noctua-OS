#include "blockcache.h"
#include "string.h"
#include "screen.h"
#include "mm/heap.h"
#include "drivers/block/blockdev.h"

static bcache_block_t bcache[BCACHE_SIZE];
static int bcache_initialized = 0;

void bcache_init(void) {
    memset(bcache, 0, sizeof(bcache));
    bcache_initialized = 1;
    screen_term_write("[CACHE] Block cache initialized (");
    char buf[8];
    int2str(BCACHE_SIZE * BCACHE_BLOCK_SIZE / 1024, buf);
    screen_term_write(buf);
    screen_term_write(" KB)\n");
}

static int bcache_find(int dev_id, uint32_t block) {
    for (int i = 0; i < BCACHE_SIZE; i++) {
        if (bcache[i].used && bcache[i].dev_id == dev_id && bcache[i].block == block) {
            bcache[i].recent = 1;
            return i;
        }
    }
    return -1;
}

static int bcache_evict(void) {
    int oldest = -1;
    int oldest_unused = 0x7FFFFFFF;

    for (int i = 0; i < BCACHE_SIZE; i++) {
        if (!bcache[i].used) return i;
        if (!bcache[i].recent && bcache[i].used) {
            if (oldest < 0 || bcache[i].used < oldest_unused) {
                oldest = i;
                oldest_unused = bcache[i].used;
            }
        }
    }

    for (int i = 0; i < BCACHE_SIZE; i++) {
        bcache[i].recent = 0;
    }

    if (oldest >= 0) {
        if (bcache[oldest].dirty) {
            block_dev_t *d = blockdev_get(bcache[oldest].dev_id);
            if (d && d->write) d->write(d->priv, bcache[oldest].block, bcache[oldest].data, 1);
        }
        bcache[oldest].used = 0;
        return oldest;
    }

    return -1;
}

uint8_t *bcache_read(int dev_id, uint32_t block) {
    if (!bcache_initialized) return NULL;

    int idx = bcache_find(dev_id, block);
    if (idx >= 0) return bcache[idx].data;

    idx = bcache_evict();
    if (idx < 0) return NULL;

    block_dev_t *dev = blockdev_get(dev_id);
    if (!dev || !dev->read || !dev->read(dev->priv, block, bcache[idx].data, 1)) {
        return NULL;
    }

    bcache[idx].used = 1;
    bcache[idx].dev_id = dev_id;
    bcache[idx].block = block;
    bcache[idx].dirty = 0;
    bcache[idx].recent = 1;

    return bcache[idx].data;
}

int bcache_write(int dev_id, uint32_t block, const uint8_t *data) {
    if (!bcache_initialized) return -1;

    int idx = bcache_find(dev_id, block);
    if (idx < 0) {
        idx = bcache_evict();
        if (idx < 0) {
            block_dev_t *d = blockdev_get(dev_id);
            if (d && d->write) return d->write(d->priv, block, data, 1);
            return -1;
        }
        bcache[idx].used = 1;
        bcache[idx].dev_id = dev_id;
        bcache[idx].block = block;
        bcache[idx].recent = 1;
    }

    memcpy(bcache[idx].data, data, BCACHE_BLOCK_SIZE);
    bcache[idx].dirty = 1;
    return 0;
}

void bcache_flush(void) {
    for (int i = 0; i < BCACHE_SIZE; i++) {
        if (bcache[i].used && bcache[i].dirty) {
            block_dev_t *d = blockdev_get(bcache[i].dev_id);
            if (d && d->write) d->write(d->priv, bcache[i].block, bcache[i].data, 1);
            bcache[i].dirty = 0;
        }
    }
}

void bcache_flush_dev(int dev_id) {
    for (int i = 0; i < BCACHE_SIZE; i++) {
        if (bcache[i].used && bcache[i].dev_id == dev_id && bcache[i].dirty) {
            block_dev_t *d = blockdev_get(dev_id);
            if (d && d->write) d->write(d->priv, bcache[i].block, bcache[i].data, 1);
            bcache[i].dirty = 0;
        }
    }
}

int bcache_get_stats(int *used, int *dirty, int *total) {
    *used = 0;
    *dirty = 0;
    *total = BCACHE_SIZE;
    for (int i = 0; i < BCACHE_SIZE; i++) {
        if (bcache[i].used) (*used)++;
        if (bcache[i].dirty) (*dirty)++;
    }
    return 0;
}

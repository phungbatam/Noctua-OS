#ifndef TVN_SLAB_H
#define TVN_SLAB_H

#include <stdint.h>

#define SLAB_PAGE_SIZE   4096
#define KMEM_MAX_CACHES  12

void slab_init(void);
void *kmem_cache_alloc(uint32_t size);
void  kmem_cache_free(void *ptr);
uint32_t kmem_cache_usage(void);

#endif

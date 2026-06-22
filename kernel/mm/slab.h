#ifndef TVN_SLAB_H
#define TVN_SLAB_H

#include <stdint.h>

#define SLAB_PAGE_SIZE   4096
#define KMEM_MAX_CACHES  12
#define SLAB_ALIGN       8
#define MAX_CPUS         4
#define SLAB_PERCPU_LIMIT 16  /* Max objects in per-CPU cache */

/* Per-CPU cache for fast allocations */
typedef struct {
    void *freelist;
    int count;
} slab_percpu_cache_t;

typedef struct slab_header slab_header_t;

void slab_init(void);
void *kmem_cache_alloc(uint32_t size);
void  kmem_cache_free(void *ptr);
uint32_t kmem_cache_usage(void);
void *kmalloc_large(uint32_t size);  /* Fallback for large allocations */
void  kfree_large(void *ptr);        /* Free large allocations */

#endif

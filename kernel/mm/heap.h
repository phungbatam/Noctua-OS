#ifndef TVN_HEAP_H
#define TVN_HEAP_H

#include <stdint.h>

/* TVN_OS Heap - đơn giản hơn slab của Linux
 * Dùng block list với first-fit */

#define HEAP_MAGIC 0x54564E48 /* "TVNH" */

typedef struct heap_block {
    uint32_t magic;
    uint32_t size;
    struct heap_block *next;
    struct heap_block *prev;
    int free;
} heap_block_t;

void heap_init(void *start, uint32_t size);
void *kmalloc(uint32_t size);
void kfree(void *ptr);
void *kcalloc(uint32_t n, uint32_t size);
void *krealloc(void *ptr, uint32_t size);
uint32_t heap_used(void);
uint32_t heap_free(void);

#endif
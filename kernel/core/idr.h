#ifndef NOCTUA_IDR_H
#define NOCTUA_IDR_H

#include <stdint.h>
#include <stddef.h>
#include "bitops.h"

#define IDR_MAX_ENTRIES 256
#define IDR_BITS(id)    BITS_TO_LONGS(IDR_MAX_ENTRIES)

typedef struct {
    void *entries[IDR_MAX_ENTRIES];
    unsigned long bitmap[IDR_BITS(0)];
    unsigned int next_cursor;
} idr_t;

static inline void idr_init(idr_t *idr) {
    for (int i = 0; i < IDR_MAX_ENTRIES; i++)
        idr->entries[i] = NULL;
    for (int i = 0; i < IDR_BITS(0); i++)
        idr->bitmap[i] = 0;
    idr->next_cursor = 0;
}

int idr_alloc(idr_t *idr, void *ptr, int start, int end);
int idr_alloc_cyclic(idr_t *idr, void *ptr, int start, int end);
void *idr_find(const idr_t *idr, unsigned long id);
void *idr_remove(idr_t *idr, unsigned long id);
void idr_destroy(idr_t *idr);
int idr_for_each(const idr_t *idr, int (*fn)(int id, void *p, void *data), void *data);

#endif

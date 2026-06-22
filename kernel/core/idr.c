#include "idr.h"
#include "spinlock.h"
#include <stddef.h>

static spinlock_t idr_global_lock;

int idr_alloc(idr_t *idr, void *ptr, int start, int end) {
    uint32_t flags;
    if (!idr || !ptr) return -1;
    if (end < 0 || end > IDR_MAX_ENTRIES) end = IDR_MAX_ENTRIES;
    if (start < 0) start = 0;

    spin_lock_irqsave(&idr_global_lock, &flags);

    for (int id = start; id < end; id++) {
        if (!test_bit(id, idr->bitmap)) {
            set_bit(id, idr->bitmap);
            idr->entries[id] = ptr;
            spin_unlock_irqrestore(&idr_global_lock, flags);
            return id;
        }
    }

    spin_unlock_irqrestore(&idr_global_lock, flags);
    return -1;
}

int idr_alloc_cyclic(idr_t *idr, void *ptr, int start, int end) {
    uint32_t flags;
    if (!idr || !ptr) return -1;
    if (end < 0 || end > IDR_MAX_ENTRIES) end = IDR_MAX_ENTRIES;
    if (start < 0) start = 0;

    spin_lock_irqsave(&idr_global_lock, &flags);

    int id = (int)idr->next_cursor;
    if (id < start || id >= end) id = start;

    int found = -1;
    for (int i = 0; i < (end - start); i++) {
        int try_id = id + i;
        if (try_id >= end) try_id = start + (try_id - end);
        if (!test_bit(try_id, idr->bitmap)) {
            set_bit(try_id, idr->bitmap);
            idr->entries[try_id] = ptr;
            idr->next_cursor = (unsigned int)(try_id + 1);
            if ((int)idr->next_cursor >= end) idr->next_cursor = (unsigned int)start;
            found = try_id;
            break;
        }
    }

    spin_unlock_irqrestore(&idr_global_lock, flags);
    return found;
}

void *idr_find(const idr_t *idr, unsigned long id) {
    if (!idr || id >= IDR_MAX_ENTRIES) return NULL;
    if (!test_bit((unsigned int)id, idr->bitmap)) return NULL;
    return idr->entries[id];
}

void *idr_remove(idr_t *idr, unsigned long id) {
    uint32_t flags;
    if (!idr || id >= IDR_MAX_ENTRIES) return NULL;

    spin_lock_irqsave(&idr_global_lock, &flags);

    if (!test_bit((unsigned int)id, idr->bitmap)) {
        spin_unlock_irqrestore(&idr_global_lock, flags);
        return NULL;
    }

    void *ptr = idr->entries[id];
    idr->entries[id] = NULL;
    clear_bit((unsigned int)id, idr->bitmap);

    spin_unlock_irqrestore(&idr_global_lock, flags);
    return ptr;
}

void idr_destroy(idr_t *idr) {
    uint32_t flags;
    if (!idr) return;

    spin_lock_irqsave(&idr_global_lock, &flags);

    for (int i = 0; i < IDR_MAX_ENTRIES; i++)
        idr->entries[i] = NULL;
    for (int i = 0; i < IDR_BITS(0); i++)
        idr->bitmap[i] = 0;
    idr->next_cursor = 0;

    spin_unlock_irqrestore(&idr_global_lock, flags);
}

int idr_for_each(const idr_t *idr, int (*fn)(int id, void *p, void *data), void *data) {
    if (!idr || !fn) return -1;

    for (int i = 0; i < IDR_MAX_ENTRIES; i++) {
        if (test_bit((unsigned int)i, idr->bitmap)) {
            int ret = fn(i, idr->entries[i], data);
            if (ret) return ret;
        }
    }
    return 0;
}

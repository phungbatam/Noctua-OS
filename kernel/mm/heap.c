#include "heap.h"
#include "string.h"

static heap_block_t *heap_first = 0;
static uint32_t heap_total = 0;
static uint32_t heap_used_bytes = 0;

void heap_init(void *start, uint32_t size) {
    heap_first = (heap_block_t *)start;
    heap_first->magic = HEAP_MAGIC;
    heap_first->size = size - sizeof(heap_block_t);
    heap_first->next = 0;
    heap_first->prev = 0;
    heap_first->free = 1;
    heap_total = size;
    heap_used_bytes = 0;
}

void *kmalloc(uint32_t size) {
    if (!heap_first || size == 0) return 0;

    /* Căn chỉnh 4 byte */
    size = (size + 3) & ~3;

    heap_block_t *curr = heap_first;
    while (curr) {
        if (curr->magic != HEAP_MAGIC) return 0;
        if (curr->free && curr->size >= size) {
            /* Split if large enough */
            if (curr->size > size + sizeof(heap_block_t) + 4) {
                heap_block_t *new_block = (heap_block_t *)((uint8_t *)(curr + 1) + size);
                new_block->magic = HEAP_MAGIC;
                new_block->size = curr->size - size - sizeof(heap_block_t);
                new_block->free = 1;
                new_block->next = curr->next;
                new_block->prev = curr;
                if (curr->next) curr->next->prev = new_block;
                curr->next = new_block;
                curr->size = size;
            }
            curr->free = 0;
            heap_used_bytes += curr->size;
            return (void *)(curr + 1);
        }
        curr = curr->next;
    }
    return 0;
}

void kfree(void *ptr) {
    if (!ptr) return;
    heap_block_t *block = (heap_block_t *)ptr - 1;
    if (block->magic != HEAP_MAGIC) return;

    block->free = 1;
    heap_used_bytes -= block->size;

    /* Merge với block sau nếu free */
    if (block->next && block->next->free) {
        block->size += sizeof(heap_block_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
    }

    /* Merge với block trước nếu free */
    if (block->prev && block->prev->free) {
        block->prev->size += sizeof(heap_block_t) + block->size;
        block->prev->next = block->next;
        if (block->next) block->next->prev = block->prev;
    }
}

void *kcalloc(uint32_t n, uint32_t size) {
    void *ptr = kmalloc(n * size);
    if (ptr) memset(ptr, 0, n * size);
    return ptr;
}

void *krealloc(void *ptr, uint32_t size) {
    if (!ptr) return kmalloc(size);
    heap_block_t *block = (heap_block_t *)ptr - 1;
    if (block->magic != HEAP_MAGIC) return 0;

    void *new_ptr = kmalloc(size);
    if (new_ptr) {
        uint32_t copy_size = block->size < size ? block->size : size;
        memcpy(new_ptr, ptr, copy_size);
        kfree(ptr);
    }
    return new_ptr;
}

uint32_t heap_used(void) { return heap_used_bytes; }
uint32_t heap_free(void) { return heap_total - sizeof(heap_block_t) - heap_used_bytes; }
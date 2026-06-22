#include "page.h"
#include "string.h"
#include "ports.h"

#define BITS_PER_BYTE 8
#define BITS_PER_WORD 32

extern char _end[];

static uint32_t *page_bitmap = 0;
static uint32_t total_pages = 0;
static uint32_t used_pages = 0;
static uint32_t bitmap_size = 0;
static uint32_t mem_start_addr = 0;

/* Buddy system free lists (based on Linux page_alloc.c) */
static struct {
    void *free_list;
    uint32_t count;
} buddy_free_lists[MAX_ORDER];

void pmem_init(uint32_t mem_upper_kb, uint32_t mem_lower_kb) {
    uint32_t total_kb = mem_lower_kb + mem_upper_kb;
    total_pages = (total_kb * 1024) / PAGE_SIZE;
    bitmap_size = (total_pages + BITS_PER_WORD - 1) / BITS_PER_WORD;

    uint32_t kernel_end = (uint32_t)_end;
    mem_start_addr = (kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    page_bitmap = (uint32_t *)mem_start_addr;
    uint32_t bitmap_bytes = bitmap_size * 4;
    memset(page_bitmap, 0, bitmap_bytes);

    uint32_t bitmap_pages = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    used_pages = mem_start_addr / PAGE_SIZE + bitmap_pages;
    for (uint32_t i = 0; i < used_pages; i++) {
        uint32_t idx = i / BITS_PER_WORD;
        uint32_t bit = i % BITS_PER_WORD;
        page_bitmap[idx] |= (1 << bit);
    }

    /* Initialize buddy system free lists */
    for (int i = 0; i < MAX_ORDER; i++) {
        buddy_free_lists[i].free_list = 0;
        buddy_free_lists[i].count = 0;
    }
}

void *pmem_alloc_page(void) {
    for (uint32_t i = used_pages - used_pages; i < total_pages; i++) {
        uint32_t idx = i / BITS_PER_WORD;
        uint32_t bit = i % BITS_PER_WORD;
        if (!(page_bitmap[idx] & (1 << bit))) {
            page_bitmap[idx] |= (1 << bit);
            used_pages++;
            return (void *)(i * PAGE_SIZE);
        }
    }
    return 0;
}

void pmem_free_page(void *addr) {
    uint32_t page_idx = (uint32_t)addr / PAGE_SIZE;
    if (page_idx >= total_pages) return;
    uint32_t idx = page_idx / BITS_PER_WORD;
    uint32_t bit = page_idx % BITS_PER_WORD;
    if (page_bitmap[idx] & (1 << bit)) {
        page_bitmap[idx] &= ~(1 << bit);
        used_pages--;
    }
}

uint32_t pmem_total_pages(void) { return total_pages; }
uint32_t pmem_free_pages(void) { return total_pages - used_pages; }
uint32_t pmem_used_pages(void) { return used_pages; }

/* Buddy system implementation based on Linux page_alloc.c */
static inline uint32_t get_buddy_index(uint32_t page_idx, int order) {
    return page_idx ^ (1 << order);
}

void *pmem_alloc_pages(int order) {
    if (order < 0 || order >= MAX_ORDER) return 0;

    /* Try to allocate from buddy free list first */
    if (buddy_free_lists[order].free_list) {
        void *page = buddy_free_lists[order].free_list;
        buddy_free_lists[order].free_list = *(void **)page;
        buddy_free_lists[order].count--;
        used_pages += (1 << order);
        return page;
    }

    /* Fall back to bitmap allocator for single pages */
    if (order == 0) {
        return pmem_alloc_page();
    }

    /* Try to split a larger block */
    for (int current_order = order + 1; current_order < MAX_ORDER; current_order++) {
        if (buddy_free_lists[current_order].free_list) {
            void *block = buddy_free_lists[current_order].free_list;
            buddy_free_lists[current_order].free_list = *(void **)block;
            buddy_free_lists[current_order].count--;

            /* Split the block down to the requested order */
            for (int split_order = current_order - 1; split_order >= order; split_order--) {
                uint32_t block_size = 1 << split_order;
                void *buddy = (void *)((uint32_t)block + (block_size * PAGE_SIZE));
                *(void **)buddy = buddy_free_lists[split_order].free_list;
                buddy_free_lists[split_order].free_list = buddy;
                buddy_free_lists[split_order].count++;
            }

            used_pages += (1 << order);
            return block;
        }
    }

    /* Fall back to allocating individual pages */
    void *page = pmem_alloc_page();
    if (page && order == 0) {
        return page;
    }

    return 0;
}

void pmem_free_pages_order(void *addr, int order) {
    if (!addr || order < 0 || order >= MAX_ORDER) return;

    uint32_t page_idx = (uint32_t)addr / PAGE_SIZE;
    if (page_idx >= total_pages) return;

    /* Mark pages as free in bitmap */
    for (uint32_t i = 0; i < (1 << order); i++) {
        uint32_t idx = page_idx + i;
        uint32_t bitmap_idx = idx / BITS_PER_WORD;
        uint32_t bit = idx % BITS_PER_WORD;
        if (page_bitmap[bitmap_idx] & (1 << bit)) {
            page_bitmap[bitmap_idx] &= ~(1 << bit);
            used_pages--;
        }
    }

    /* Try to merge with buddy */
    uint32_t current_idx = page_idx;
    int current_order = order;

    while (current_order < MAX_ORDER - 1) {
        uint32_t buddy_idx = get_buddy_index(current_idx, current_order);
        uint32_t buddy_bitmap_idx = buddy_idx / BITS_PER_WORD;
        uint32_t buddy_bit = buddy_idx % BITS_PER_WORD;

        /* Check if buddy is free */
        if (page_bitmap[buddy_bitmap_idx] & (1 << buddy_bit)) {
            break; /* Buddy is in use, cannot merge */
        }

        /* Remove buddy from free list if it's there */
        void *buddy_addr = (void *)(buddy_idx * PAGE_SIZE);
        /* This is a simplified check - in a real implementation we'd need to
         * search and remove from the appropriate free list */

        /* Merge */
        current_idx = (current_idx < buddy_idx) ? current_idx : buddy_idx;
        current_order++;
    }

    /* Add to appropriate free list */
    void *block = (void *)(current_idx * PAGE_SIZE);
    *(void **)block = buddy_free_lists[current_order].free_list;
    buddy_free_lists[current_order].free_list = block;
    buddy_free_lists[current_order].count++;
}
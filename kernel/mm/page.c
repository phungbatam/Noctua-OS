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
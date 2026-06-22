#ifndef NOCTUA_PAGE_H
#define NOCTUA_PAGE_H

#include <stdint.h>

#define PAGE_SIZE       4096
#define PAGE_SHIFT      12
#define PAGE_MASK       0xFFFFF000

/* Bitmap quản lý physical page frame */
/* TVN_OS dùng bitmap đơn giản, khác Linux dùng buddy system phức tạp */

void pmem_init(uint32_t mem_upper_kb, uint32_t mem_lower_kb);
void *pmem_alloc_page(void);
void pmem_free_page(void *addr);
uint32_t pmem_total_pages(void);
uint32_t pmem_free_pages(void);
uint32_t pmem_used_pages(void);

#endif
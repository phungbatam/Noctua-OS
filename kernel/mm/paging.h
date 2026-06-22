#ifndef TVN_PAGING_H
#define TVN_PAGING_H

#include <stdint.h>

/* TVN_OS paging: Page Directory + Page Table kiểu x86 */
/* Khác Linux: TVN_OS dùng identity mapping + heap mapping đơn giản */

#define PD_IDX(vaddr) ((uint32_t)(vaddr) >> 22)
#define PT_IDX(vaddr) (((uint32_t)(vaddr) >> 12) & 0x3FF)

#define PAGE_PRESENT   (1 << 0)
#define PAGE_WRITE     (1 << 1)
#define PAGE_USER      (1 << 2)
#define PAGE_4MB       (1 << 7)

void paging_init(void);
void paging_map_page(void *virt, void *phys, uint32_t flags);
void paging_map_region(void *virt, void *phys, uint32_t size, uint32_t flags);
void paging_unmap_page(void *virt);
void *paging_get_phys(void *virt);
void paging_enable(void);

/* Phân vùng địa chỉ ảo TVN_OS:
 * 0x00000000 - 0x003FFFFF: Identity map (4MB cho kernel)
 * 0x00400000 - 0x00BFFFFF: Heap area (8MB)
 * 0x00C00000 - 0x00FFFFFF: Reserved
 * 0xF0000000 - 0xFFFFFFFF: Page directory/table self-ref
 */

#define KERNEL_HEAP_START  0x00400000
#define KERNEL_HEAP_SIZE   (8 * 1024 * 1024)
#define KERNEL_HEAP_PAGES  (KERNEL_HEAP_SIZE / PAGE_SIZE)

#endif
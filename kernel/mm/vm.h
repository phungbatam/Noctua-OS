#ifndef NOCTUA_VM_H
#define NOCTUA_VM_H

#include <stdint.h>

#define VM_USER_START    0x40000000
#define VM_USER_HEAP     0x80000000
#define VM_USER_STACK    0xC0000000
#define VM_KERNEL_START  0x00000000
#define VM_PAGES_PER_DIR 1024
#define VM_PAGE_SIZE     4096

/* Page table entry flags */
#define VM_PAGE_PRESENT   0x01
#define VM_PAGE_WRITE     0x02
#define VM_PAGE_USER      0x04
#define VM_PAGE_ACCESSED  0x20
#define VM_PAGE_DIRTY     0x40

/* Page directory entry flags (same as page table) */
#define VM_DIR_PRESENT    0x01
#define VM_DIR_WRITE      0x02
#define VM_DIR_USER       0x04

/* Per-process address space */
typedef struct vm_space {
    uint32_t *page_dir;       /* Physical address of page directory */
    uint32_t page_dir_virt;   /* Virtual address of page directory */
    uint32_t heap_start;
    uint32_t heap_end;
    uint32_t brk;
    int refcount;
} vm_space_t;

/* Functions */
void vm_init(void);
vm_space_t *vm_create_space(void);
void vm_destroy_space(vm_space_t *space);
int vm_map_page(vm_space_t *space, uint32_t virt, uint32_t phys, uint32_t flags);
int vm_unmap_page(vm_space_t *space, uint32_t virt);
int vm_map_range(vm_space_t *space, uint32_t virt, uint32_t phys, uint32_t size, uint32_t flags);
uint32_t vm_get_phys(vm_space_t *space, uint32_t virt);
void vm_switch_space(vm_space_t *space);
vm_space_t *vm_clone_space(vm_space_t *src);
void vm_dump(vm_space_t *space);

#endif

#include "vm.h"
#include "mm/page.h"
#include "mm/paging.h"
#include "string.h"
#include "screen.h"
#include "mm/heap.h"

static vm_space_t *kernel_space = NULL;
static vm_space_t *current_space = NULL;

static uint32_t read_cr3(void) {
    uint32_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

static void write_cr3(uint32_t cr3) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

static uint32_t *get_page_dir_virt(uint32_t phys_addr) {
    return (uint32_t *)(0xC0000000 | (phys_addr >> 20));
}

static void invlpg(uint32_t addr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

void vm_init(void) {
    kernel_space = (vm_space_t *)kmalloc(sizeof(vm_space_t));
    memset(kernel_space, 0, sizeof(vm_space_t));
    kernel_space->page_dir = (uint32_t *)(uint32_t)read_cr3();
    kernel_space->page_dir_virt = (uint32_t)kernel_space->page_dir;
    kernel_space->refcount = 1;
    current_space = kernel_space;
    screen_term_write("[VM] Virtual memory manager initialized\n");
}

vm_space_t *vm_create_space(void) {
    vm_space_t *space = (vm_space_t *)kmalloc(sizeof(vm_space_t));
    memset(space, 0, sizeof(vm_space_t));

    uint32_t pd_phys = (uint32_t)pmem_alloc_page();
    if (!pd_phys) { kfree(space); return NULL; }
    memset((void *)pd_phys, 0, PAGE_SIZE);

    space->page_dir = (uint32_t *)pd_phys;
    space->page_dir_virt = (uint32_t)pd_phys + 0xC0000000;
    space->heap_start = VM_USER_HEAP;
    space->heap_end = VM_USER_HEAP;
    space->brk = VM_USER_HEAP;
    space->refcount = 1;

    uint32_t *kernel_pd = (uint32_t *)kernel_space->page_dir_virt;
    for (int i = 768; i < 1024; i++) {
        space->page_dir[i] = kernel_pd[i];
    }

    return space;
}

void vm_destroy_space(vm_space_t *space) {
    if (!space) return;
    if (--space->refcount > 0) return;

    for (int i = 0; i < 768; i++) {
        if (space->page_dir[i] & VM_DIR_PRESENT) {
            uint32_t pt_phys = space->page_dir[i] & ~0xFFF;
            uint32_t *pt = (uint32_t *)(pt_phys + 0xC0000000);
            for (int j = 0; j < 1024; j++) {
                if (pt[j] & VM_PAGE_PRESENT) {
                    pmem_free_page((void *)(pt[j] & ~0xFFF));
                }
            }
            pmem_free_page((void *)pt_phys);
        }
    }
    pmem_free_page((void *)(uint32_t)space->page_dir);
    kfree(space);
}

static uint32_t *vm_get_or_create_pt(vm_space_t *space, uint32_t virt) {
    uint32_t pd_index = virt >> 22;
    uint32_t *pd = (uint32_t *)(space->page_dir_virt);

    if (pd[pd_index] & VM_DIR_PRESENT) {
        uint32_t pt_phys = pd[pd_index] & ~0xFFF;
        return (uint32_t *)(pt_phys + 0xC0000000);
    }

    uint32_t pt_phys = (uint32_t)pmem_alloc_page();
    if (!pt_phys) return NULL;
    memset((void *)pt_phys, 0, PAGE_SIZE);

    pd[pd_index] = pt_phys | VM_DIR_PRESENT | VM_DIR_WRITE | VM_DIR_USER;
    return (uint32_t *)(pt_phys + 0xC0000000);
}

int vm_map_page(vm_space_t *space, uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t *pt = vm_get_or_create_pt(space, virt);
    if (!pt) return -1;

    uint32_t pt_index = (virt >> 12) & 0x3FF;
    pt[pt_index] = (phys & ~0xFFF) | (flags & 0xFFF) | VM_PAGE_PRESENT;

    if (space == current_space) {
        invlpg(virt);
    }
    return 0;
}

int vm_unmap_page(vm_space_t *space, uint32_t virt) {
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    uint32_t *pd = (uint32_t *)space->page_dir_virt;

    if (!(pd[pd_index] & VM_DIR_PRESENT)) return -1;

    uint32_t *pt = (uint32_t *)((pd[pd_index] & ~0xFFF) + 0xC0000000);
    if (!(pt[pt_index] & VM_PAGE_PRESENT)) return -1;

    pt[pt_index] = 0;
    if (space == current_space) invlpg(virt);
    return 0;
}

int vm_map_range(vm_space_t *space, uint32_t virt, uint32_t phys, uint32_t size, uint32_t flags) {
    for (uint32_t off = 0; off < size; off += VM_PAGE_SIZE) {
        if (vm_map_page(space, virt + off, phys + off, flags) < 0) return -1;
    }
    return 0;
}

uint32_t vm_get_phys(vm_space_t *space, uint32_t virt) {
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    uint32_t *pd = (uint32_t *)space->page_dir_virt;

    if (!(pd[pd_index] & VM_DIR_PRESENT)) return 0;

    uint32_t *pt = (uint32_t *)((pd[pd_index] & ~0xFFF) + 0xC0000000);
    if (!(pt[pt_index] & VM_PAGE_PRESENT)) return 0;

    return (pt[pt_index] & ~0xFFF) | (virt & 0xFFF);
}

void vm_switch_space(vm_space_t *space) {
    if (!space || space == current_space) return;
    current_space = space;
    write_cr3((uint32_t)space->page_dir);
}

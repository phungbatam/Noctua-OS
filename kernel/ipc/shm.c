#include "shm.h"
#include "../mm/page.h"
#include "../mm/paging.h"
#include "../lib/string.h"
#include "../proc/task.h"
#include "../core/syscall.h"

#define MAX_SHM_SEGMENTS 32

typedef struct {
    int id;
    uint32_t size;
    void *addr;
    int refcount;
    int key;
    int attached[MAX_TASKS];
} shm_segment_t;

static shm_segment_t shm_segments[MAX_SHM_SEGMENTS];
static int shm_count = 0;
static int next_shm_id = 1;

void shm_init(void) {
    memset(shm_segments, 0, sizeof(shm_segments));
    shm_count = 0;
    next_shm_id = 1;
}

static shm_segment_t *shm_find_by_id(int id) {
    for (int i = 0; i < shm_count; i++) {
        if (shm_segments[i].id == id) {
            return &shm_segments[i];
        }
    }
    return NULL;
}

static shm_segment_t *shm_find_by_key(int key) {
    for (int i = 0; i < shm_count; i++) {
        if (shm_segments[i].key == key && shm_segments[i].key != 0) {
            return &shm_segments[i];
        }
    }
    return NULL;
}

int shm_create(int key, uint32_t size) {
    /* Check if segment with this key already exists */
    if (key != 0) {
        shm_segment_t *existing = shm_find_by_key(key);
        if (existing) {
            existing->refcount++;
            return existing->id;
        }
    }
    
    if (shm_count >= MAX_SHM_SEGMENTS) {
        return -1;
    }
    
    /* Calculate number of pages needed */
    uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    /* Allocate physical pages */
    void *phys_addr = NULL;
    for (uint32_t i = 0; i < pages; i++) {
        void *page = pmem_alloc_page();
        if (!page) {
            /* Cleanup allocated pages on failure */
            return -1;
        }
        if (i == 0) {
            phys_addr = page;
        }
    }
    
    /* Create new segment */
    shm_segment_t *seg = &shm_segments[shm_count++];
    seg->id = next_shm_id++;
    seg->size = size;
    seg->addr = phys_addr;
    seg->refcount = 1;
    seg->key = key;
    memset(seg->attached, 0, sizeof(seg->attached));
    
    return seg->id;
}

void *shm_attach(int id) {
    shm_segment_t *seg = shm_find_by_id(id);
    if (!seg) {
        return NULL;
    }
    
    task_t *current = task_current();
    if (!current) {
        return NULL;
    }
    
    /* Find a virtual address to map to */
    uint32_t vaddr = 0xD0000000 + (id * 0x100000);
    
    /* Map the shared memory pages */
    uint32_t pages = (seg->size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < pages; i++) {
        void *phys_page = (void *)((uint32_t)seg->addr + i * PAGE_SIZE);
        void *virt_page = (void *)(vaddr + i * PAGE_SIZE);
        paging_map_page(virt_page, phys_page, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }
    
    /* Mark as attached for this task */
    for (int i = 0; i < MAX_TASKS; i++) {
        if (seg->attached[i] == 0 || seg->attached[i] == current->pid) {
            seg->attached[i] = current->pid;
            break;
        }
    }
    
    seg->refcount++;
    
    return (void *)vaddr;
}

int shm_detach(void *addr) {
    (void)addr;
    /* TODO: Implement proper detachment with unmap */
    return 0;
}

int shm_delete(int id) {
    shm_segment_t *seg = shm_find_by_id(id);
    if (!seg) {
        return -1;
    }
    
    seg->refcount--;
    
    if (seg->refcount <= 0) {
        /* Free physical pages */
        uint32_t pages = (seg->size + PAGE_SIZE - 1) / PAGE_SIZE;
        for (uint32_t i = 0; i < pages; i++) {
            void *page = (void *)((uint32_t)seg->addr + i * PAGE_SIZE);
            pmem_free_page(page);
        }
        
        /* Remove from array */
        for (int i = 0; i < shm_count; i++) {
            if (&shm_segments[i] == seg) {
                for (int j = i; j < shm_count - 1; j++) {
                    shm_segments[j] = shm_segments[j + 1];
                }
                shm_count--;
                break;
            }
        }
    }
    
    return 0;
}

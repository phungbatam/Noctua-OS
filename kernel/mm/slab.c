#include "slab.h"
#include "page.h"
#include "string.h"

/* Forward declaration needed because slab_header_t references kmem_cache_t */
typedef struct kmem_cache kmem_cache_t;

typedef struct slab_header {
    struct slab_header *next;
    void              *free_list;
    uint16_t           total_objs;
    uint16_t           free_objs;
    uint16_t           objsize;
    uint32_t           cpu_id;
    kmem_cache_t       *cache;      /* Back-pointer to owning cache */
} slab_header_t;

typedef struct kmem_cache {
    const char          *name;
    uint32_t             objsize;
    uint32_t             aligned_objsize;
    slab_header_t       *slabs_partial;
    slab_header_t       *slabs_full;
    slab_header_t       *slabs_free;
    uint32_t             active_objs;
    slab_percpu_cache_t  cpu_cache[MAX_CPUS];
} kmem_cache_t;

static kmem_cache_t caches[KMEM_MAX_CACHES];
static int cache_count = 0;
static int current_cpu = 0;

/* Hash table for fast slab lookup: maps page_addr -> (cache, slab_header) */
#define SLAB_HASH_SIZE 256
static struct {
    uint32_t page_addr;
    kmem_cache_t *cache;
    slab_header_t *slab;
} slab_hash[SLAB_HASH_SIZE];

static uint32_t slab_hash_func(uint32_t page_addr) {
    return (page_addr >> 12) % SLAB_HASH_SIZE;
}

static void slab_hash_insert(uint32_t page_addr, kmem_cache_t *cache, slab_header_t *slab) {
    uint32_t idx = slab_hash_func(page_addr);
    slab_hash[idx].page_addr = page_addr;
    slab_hash[idx].cache = cache;
    slab_hash[idx].slab = slab;
}

/* Find slab by page address using hash table */
static int slab_find_by_page(uint32_t page_addr, kmem_cache_t **out_cache, slab_header_t **out_slab) {
    uint32_t idx = slab_hash_func(page_addr);
    if (slab_hash[idx].page_addr == page_addr) {
        *out_cache = slab_hash[idx].cache;
        *out_slab = slab_hash[idx].slab;
        return 1;
    }
    return 0;
}

static uint32_t slab_align_size(uint32_t size) {
    return (size + SLAB_ALIGN - 1) & ~(SLAB_ALIGN - 1);
}

static void slab_fill_free_list(slab_header_t *sh, uint32_t objsize) {
    void *obj = (void *)((uint32_t)sh + sizeof(slab_header_t));
    void *end = (void *)((uint32_t)sh + SLAB_PAGE_SIZE);
    void *prev = 0;
    sh->total_objs = 0;
    sh->objsize = objsize;
    while ((uint32_t)obj + objsize <= (uint32_t)end) {
        if (prev) *(void **)prev = obj;
        else sh->free_list = obj;
        prev = obj;
        obj = (void *)((uint32_t)obj + objsize);
        sh->total_objs++;
    }
    if (prev) *(void **)prev = 0;
    sh->free_objs = sh->total_objs;
}

static slab_header_t *slab_alloc_new(kmem_cache_t *cache) {
    void *page = pmem_alloc_page();
    if (!page) return 0;
    slab_header_t *sh = (slab_header_t *)page;
    sh->next = 0;
    sh->cpu_id = current_cpu;
    sh->cache = cache;
    slab_fill_free_list(sh, cache->aligned_objsize);

    /* Insert into hash table for fast lookup */
    slab_hash_insert((uint32_t)page, cache, sh);

    return sh;
}

static kmem_cache_t *cache_for_size(uint32_t size) {
    for (int i = 0; i < cache_count; i++) {
        if (caches[i].objsize >= size) return &caches[i];
    }
    return 0;
}

void slab_init(void) {
    static const uint32_t sizes[] = {32, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048};
    static const char *names[] = {
        "kmem-32", "kmem-64", "kmem-96", "kmem-128", "kmem-192", "kmem-256",
        "kmem-384", "kmem-512", "kmem-768", "kmem-1K", "kmem-1.5K", "kmem-2K"
    };
    cache_count = KMEM_MAX_CACHES;
    for (int i = 0; i < KMEM_MAX_CACHES; i++) {
        caches[i].name = names[i];
        caches[i].objsize = sizes[i];
        caches[i].aligned_objsize = slab_align_size(sizes[i]);
        caches[i].slabs_partial = 0;
        caches[i].slabs_full = 0;
        caches[i].slabs_free = 0;
        caches[i].active_objs = 0;
        /* Initialize per-CPU caches */
        for (int cpu = 0; cpu < MAX_CPUS; cpu++) {
            caches[i].cpu_cache[cpu].freelist = 0;
            caches[i].cpu_cache[cpu].count = 0;
        }
        slab_header_t *sh = slab_alloc_new(&caches[i]);
        if (sh) {
            sh->next = caches[i].slabs_partial;
            caches[i].slabs_partial = sh;
        }
    }
}

void *kmem_cache_alloc(uint32_t size) {
    if (size == 0) return 0;
    kmem_cache_t *cache = cache_for_size(size);
    if (!cache) return 0;

    /* Try per-CPU cache first (fast path) */
    slab_percpu_cache_t *cpu_cache = &cache->cpu_cache[current_cpu];
    if (cpu_cache->freelist) {
        void *obj = cpu_cache->freelist;
        cpu_cache->freelist = *(void **)obj;
        cpu_cache->count--;
        cache->active_objs++;
        memset(obj, 0, cache->objsize);
        return obj;
    }

    /* Fall back to slab lists (slow path) */
    slab_header_t *sh = cache->slabs_partial;
    if (!sh) {
        sh = cache->slabs_free;
        if (sh) {
            cache->slabs_free = sh->next;
            cache->slabs_partial = sh;
            sh->next = 0;
        } else {
            sh = slab_alloc_new(cache);
            if (!sh) return 0;
            sh->next = cache->slabs_partial;
            cache->slabs_partial = sh;
        }
    }

    void *obj = sh->free_list;
    if (!obj) return 0;

    sh->free_list = *(void **)obj;
    sh->free_objs--;
    cache->active_objs++;

    if (sh->free_objs == 0) {
        cache->slabs_partial = sh->next;
        sh->next = cache->slabs_full;
        cache->slabs_full = sh;
    }

    memset(obj, 0, cache->objsize);
    return obj;
}

/* Check if pointer is already in a freelist (double-free detection) */
static int slab_is_in_freelist(kmem_cache_t *cache, slab_header_t *sh, void *ptr) {
    /* Check per-CPU cache */
    for (int cpu = 0; cpu < MAX_CPUS; cpu++) {
        void *curr = cache->cpu_cache[cpu].freelist;
        while (curr) {
            if (curr == ptr) return 1;
            curr = *(void **)curr;
        }
    }
    /* Check slab free list */
    void *curr = sh->free_list;
    while (curr) {
        if (curr == ptr) return 1;
        curr = *(void **)curr;
    }
    return 0;
}

void kmem_cache_free(void *ptr) {
    if (!ptr) return;

    uint32_t addr = (uint32_t)ptr;
    uint32_t page_addr = addr & 0xFFFFF000;

    /* Use hash table for O(1) lookup */
    kmem_cache_t *cache;
    slab_header_t *sh;
    if (!slab_find_by_page(page_addr, &cache, &sh)) {
        /* Slab not found in hash table - invalid free */
        return;
    }

    /* Double-free detection */
    if (slab_is_in_freelist(cache, sh, ptr)) {
        /* Double free detected - in a real kernel we'd panic or warn here */
        return;
    }

    /* Try to add to per-CPU cache first (fast path) */
    slab_percpu_cache_t *cpu_cache = &cache->cpu_cache[current_cpu];
    if (cpu_cache->count < SLAB_PERCPU_LIMIT) {
        *(void **)ptr = cpu_cache->freelist;
        cpu_cache->freelist = ptr;
        cpu_cache->count++;
        cache->active_objs--;
        return;
    }

    /* Fall back to slab (slow path) */
    *(void **)ptr = sh->free_list;
    sh->free_list = ptr;
    sh->free_objs++;
    cache->active_objs--;

    int was_full = (sh->free_objs == 1);
    if (was_full) {
        slab_header_t **pp = &cache->slabs_full;
        while (*pp) {
            if (*pp == sh) { *pp = sh->next; break; }
            pp = &(*pp)->next;
        }
        sh->next = cache->slabs_partial;
        cache->slabs_partial = sh;
    }

    if (sh->free_objs == sh->total_objs) {
        slab_header_t **pp = &cache->slabs_partial;
        while (*pp) {
            if (*pp == sh) { *pp = sh->next; break; }
            pp = &(*pp)->next;
        }
        sh->next = cache->slabs_free;
        cache->slabs_free = sh;
    }
}

uint32_t kmem_cache_usage(void) {
    uint32_t total = 0;
    for (int i = 0; i < cache_count; i++) {
        total += caches[i].active_objs * caches[i].objsize;
    }
    return total;
}

/* Large allocation fallback: use page allocator directly for sizes > 2KB */
void *kmalloc_large(uint32_t size) {
    if (size == 0) return 0;

    /* Calculate order needed (round up to power of 2 pages) */
    uint32_t order = 0;
    uint32_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    while ((1U << order) < pages_needed && order < MAX_ORDER - 1) {
        order++;
    }

    void *ptr = pmem_alloc_pages(order);
    if (!ptr) return 0;

    /* Store order in first 4 bytes for free */
    *(uint32_t *)ptr = order;
    return (void *)((uint32_t)ptr + sizeof(uint32_t));
}

void kfree_large(void *ptr) {
    if (!ptr) return;

    /* Retrieve order from before the pointer */
    void *page_addr = (void *)((uint32_t)ptr - sizeof(uint32_t));
    uint32_t order = *(uint32_t *)page_addr;

    if (order >= MAX_ORDER) return;

    pmem_free_pages_order(page_addr, order);
}

#include "slab.h"
#include "page.h"
#include "string.h"

typedef struct slab_header {
    struct slab_header *next;
    void              *free_list;
    uint16_t           total_objs;
    uint16_t           free_objs;
    uint16_t           objsize;
} slab_header_t;

typedef struct kmem_cache {
    const char   *name;
    uint32_t      objsize;
    slab_header_t *slabs_partial;
    slab_header_t *slabs_full;
    slab_header_t *slabs_free;
    uint32_t      active_objs;
} kmem_cache_t;

static kmem_cache_t caches[KMEM_MAX_CACHES];
static int cache_count = 0;

static void slab_fill_free_list(slab_header_t *sh) {
    void *obj = (void *)((uint32_t)sh + sizeof(slab_header_t));
    void *end = (void *)((uint32_t)sh + SLAB_PAGE_SIZE);
    void *prev = 0;
    sh->total_objs = 0;
    while ((uint32_t)obj + sh->objsize <= (uint32_t)end) {
        if (prev) *(void **)prev = obj;
        else sh->free_list = obj;
        prev = obj;
        obj = (void *)((uint32_t)obj + sh->objsize);
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
    sh->objsize = cache->objsize;
    slab_fill_free_list(sh);
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
        caches[i].slabs_partial = 0;
        caches[i].slabs_full = 0;
        caches[i].slabs_free = 0;
        caches[i].active_objs = 0;
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

void kmem_cache_free(void *ptr) {
    if (!ptr) return;

    uint32_t addr = (uint32_t)ptr;
    uint32_t page_addr = addr & 0xFFFFF000;

    for (int i = 0; i < cache_count; i++) {
        kmem_cache_t *cache = &caches[i];
        slab_header_t *sh;
        int found = 0;

        sh = cache->slabs_full;
        while (sh) {
            if ((uint32_t)sh == page_addr) { found = 1; break; }
            sh = sh->next;
        }
        if (!found) {
            sh = cache->slabs_partial;
            while (sh) {
                if ((uint32_t)sh == page_addr) { found = 1; break; }
                sh = sh->next;
            }
        }
        if (!found) {
            sh = cache->slabs_free;
            while (sh) {
                if ((uint32_t)sh == page_addr) { found = 1; break; }
                sh = sh->next;
            }
        }

        if (found) {
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
            return;
        }
    }
}

uint32_t kmem_cache_usage(void) {
    uint32_t total = 0;
    for (int i = 0; i < cache_count; i++) {
        total += caches[i].active_objs * caches[i].objsize;
    }
    return total;
}

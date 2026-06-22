#ifndef NOCTUA_HLIST_H
#define NOCTUA_HLIST_H

#include <stddef.h>
#include "list.h"

struct hlist_head {
    struct hlist_node *first;
};

struct hlist_node {
    struct hlist_node *next, **pprev;
};

#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = HLIST_HEAD_INIT
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)

static inline void INIT_HLIST_NODE(struct hlist_node *h) {
    h->next = NULL;
    h->pprev = NULL;
}

static inline int hlist_unhashed(const struct hlist_node *h) {
    return !h->pprev;
}

static inline int hlist_empty(const struct hlist_head *h) {
    return !h->first;
}

static inline void __hlist_del(struct hlist_node *n) {
    struct hlist_node *next = n->next;
    struct hlist_node **pprev = n->pprev;
    *pprev = next;
    if (next)
        next->pprev = pprev;
}

static inline void hlist_del(struct hlist_node *n) {
    __hlist_del(n);
    n->next = NULL;
    n->pprev = NULL;
}

static inline void hlist_del_init(struct hlist_node *n) {
    if (!hlist_unhashed(n)) {
        __hlist_del(n);
        INIT_HLIST_NODE(n);
    }
}

static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h) {
    struct hlist_node *first = h->first;
    n->next = first;
    if (first)
        first->pprev = &n->next;
    h->first = n;
    n->pprev = &h->first;
}

static inline void hlist_add_before(struct hlist_node *n, struct hlist_node *next) {
    n->pprev = next->pprev;
    n->next = next;
    next->pprev = &n->next;
    *(n->pprev) = n;
}

static inline void hlist_add_behind(struct hlist_node *n, struct hlist_node *prev) {
    n->next = prev->next;
    prev->next = n;
    n->pprev = &prev->next;
    if (n->next)
        n->next->pprev = &n->next;
}

#define hlist_entry(ptr, type, member) container_of(ptr, type, member)

#define hlist_for_each(pos, head) \
    for (pos = (head)->first; pos; pos = pos->next)

#define hlist_for_each_safe(pos, n, head) \
    for (pos = (head)->first; pos && ({ n = pos->next; 1; }); pos = n)

#define hlist_entry_safe(ptr, type, member) \
    ({ typeof(ptr) __ptr = (ptr); \
       __ptr ? hlist_entry(__ptr, type, member) : NULL; })

#define hlist_for_each_entry(pos, head, member) \
    for (pos = hlist_entry_safe((head)->first, typeof(*(pos)), member); \
         pos; \
         pos = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

#define hlist_for_each_entry_safe(pos, n, head, member) \
    for (pos = hlist_entry_safe((head)->first, typeof(*pos), member); \
         pos && ({ n = pos->member.next; 1; }); \
         pos = hlist_entry_safe(n, typeof(*pos), member))

#endif

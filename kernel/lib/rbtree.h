#ifndef NOCTUA_RBTREE_H
#define NOCTUA_RBTREE_H

#include <stdint.h>
#include <stddef.h>
#include "compiler.h"

struct rb_node {
    unsigned long __rb_parent_color;
    struct rb_node *rb_right;
    struct rb_node *rb_left;
} __aligned(sizeof(long));

struct rb_root {
    struct rb_node *rb_node;
};

struct rb_root_cached {
    struct rb_root rb_root;
    struct rb_node *rb_leftmost;
};

#define RB_ROOT (struct rb_root) { NULL }
#define RB_ROOT_CACHED (struct rb_root_cached) { { NULL }, NULL }

#define rb_parent(r)   ((struct rb_node *)((r)->__rb_parent_color & ~3))
#define rb_color(r)    ((r)->__rb_parent_color & 1)
#define rb_is_red(r)   (!rb_color(r))
#define rb_is_black(r)  rb_color(r)
#define rb_set_red(n)   do { (n)->__rb_parent_color &= ~1UL; } while (0)
#define rb_set_black(n) do { (n)->__rb_parent_color |= 1UL; } while (0)

static inline void rb_set_color(struct rb_node *n, int color) {
    n->__rb_parent_color = (n->__rb_parent_color & ~1UL) | (unsigned int)color;
}

static inline void rb_set_parent(struct rb_node *rb, struct rb_node *p) {
    rb->__rb_parent_color = (rb->__rb_parent_color & 3) | (unsigned long)p;
}

static inline void rb_set_parent_color(struct rb_node *rb, struct rb_node *p, int color) {
    rb->__rb_parent_color = (unsigned long)p | (unsigned int)color;
}

#define RB_EMPTY_ROOT(root) ((root)->rb_node == NULL)
#define RB_EMPTY_NODE(node) ((node)->__rb_parent_color == (unsigned long)(node))
#define RB_CLEAR_NODE(node) ((node)->__rb_parent_color = (unsigned long)(node))

#define rb_entry(ptr, type, member) container_of(ptr, type, member)

extern void rb_insert_color(struct rb_node *, struct rb_root *);
extern void rb_erase(struct rb_node *, struct rb_root *);
extern struct rb_node *rb_next(const struct rb_node *);
extern struct rb_node *rb_prev(const struct rb_node *);
extern struct rb_node *rb_first(const struct rb_root *);
extern struct rb_node *rb_last(const struct rb_root *);

static inline void rb_link_node(struct rb_node *node, struct rb_node *parent, struct rb_node **rb_link) {
    node->__rb_parent_color = (unsigned long)parent;
    node->rb_left = node->rb_right = NULL;
    *rb_link = node;
}

#define rb_first_cached(root) ((root)->rb_leftmost)

static inline void rb_insert_color_cached(struct rb_node *node, struct rb_root_cached *root, int leftmost) {
    if (leftmost)
        root->rb_leftmost = node;
    rb_insert_color(node, &root->rb_root);
}

static inline struct rb_node *rb_erase_cached(struct rb_node *node, struct rb_root_cached *root) {
    if (root->rb_leftmost == node)
        root->rb_leftmost = rb_next(node);
    rb_erase(node, &root->rb_root);
    return node;
}

#endif

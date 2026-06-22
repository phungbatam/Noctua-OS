#ifndef NOCTUA_BUG_H
#define NOCTUA_BUG_H

#include "compiler.h"

#define BUG() do { \
    printk("BUG: at %s:%d", __FILE__, __LINE__); \
    for (;;) __asm__ volatile("hlt"); \
} while (0)

#define BUG_ON(cond) do { \
    if (unlikely(cond)) { \
        printk("BUG_ON: condition failed at %s:%d", __FILE__, __LINE__); \
        for (;;) __asm__ volatile("hlt"); \
    } \
} while (0)

#define WARN_ON(cond) do { \
    if (unlikely(cond)) { \
        printk("WARNING: at %s:%d", __FILE__, __LINE__); \
    } \
} while (0)

#define WARN(cond, msg) do { \
    if (unlikely(cond)) { \
        printk("WARNING: %s at %s:%d", (msg), __FILE__, __LINE__); \
    } \
} while (0)

#endif

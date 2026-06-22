#ifndef NOCTUA_BITOPS_H
#define NOCTUA_BITOPS_H

#include <stdint.h>

#define BIT(nr)         (1UL << (nr))
#define BIT_ULL(nr)     (1ULL << (nr))
#define BIT_MASK(nr)    (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)    ((nr) / BITS_PER_LONG)

#define BITS_PER_LONG   32
#define BITS_PER_BYTE   8
#define BITS_TO_LONGS(nr) (((nr) + BITS_PER_LONG - 1) / BITS_PER_LONG)

static inline void set_bit(unsigned int nr, volatile unsigned long *addr) {
    __asm__ volatile("lock orl %1, %0"
                     : "+m" (*(addr + BIT_WORD(nr)))
                     : "ir" (BIT_MASK(nr))
                     : "memory");
}

static inline void clear_bit(unsigned int nr, volatile unsigned long *addr) {
    __asm__ volatile("lock andl %1, %0"
                     : "+m" (*(addr + BIT_WORD(nr)))
                     : "ir" (~BIT_MASK(nr))
                     : "memory");
}

static inline void change_bit(unsigned int nr, volatile unsigned long *addr) {
    __asm__ volatile("lock xorl %1, %0"
                     : "+m" (*(addr + BIT_WORD(nr)))
                     : "ir" (BIT_MASK(nr))
                     : "memory");
}

static inline int test_bit(unsigned int nr, const volatile unsigned long *addr) {
    return 1UL & (addr[BIT_WORD(nr)] >> (nr % BITS_PER_LONG));
}

static inline int test_and_set_bit(unsigned int nr, volatile unsigned long *addr) {
    int old;
    __asm__ volatile("lock btsl %2, %0; sbbl %1, %1"
                     : "+m" (*(addr + BIT_WORD(nr))), "=r" (old)
                     : "Ir" (nr % BITS_PER_LONG)
                     : "memory");
    return old;
}

static inline int test_and_clear_bit(unsigned int nr, volatile unsigned long *addr) {
    int old;
    __asm__ volatile("lock btrl %2, %0; sbbl %1, %1"
                     : "+m" (*(addr + BIT_WORD(nr))), "=r" (old)
                     : "Ir" (nr % BITS_PER_LONG)
                     : "memory");
    return old;
}

static inline unsigned long find_first_bit(const unsigned long *addr, unsigned long size) {
    for (unsigned long i = 0; i < size; i++) {
        if (addr[i / BITS_PER_LONG] & (1UL << (i % BITS_PER_LONG)))
            return i;
    }
    return size;
}

static inline unsigned long find_next_bit(const unsigned long *addr, unsigned long size, unsigned long offset) {
    for (unsigned long i = offset; i < size; i++) {
        if (addr[i / BITS_PER_LONG] & (1UL << (i % BITS_PER_LONG)))
            return i;
    }
    return size;
}

static inline unsigned long find_first_zero_bit(const unsigned long *addr, unsigned long size) {
    for (unsigned long i = 0; i < size; i++) {
        if (!(addr[i / BITS_PER_LONG] & (1UL << (i % BITS_PER_LONG))))
            return i;
    }
    return size;
}

static inline unsigned long find_next_zero_bit(const unsigned long *addr, unsigned long size, unsigned long offset) {
    for (unsigned long i = offset; i < size; i++) {
        if (!(addr[i / BITS_PER_LONG] & (1UL << (i % BITS_PER_LONG))))
            return i;
    }
    return size;
}

static inline int fls(unsigned int x) {
    return x ? sizeof(x) * 8 - __builtin_clz(x) : 0;
}

static inline int ffs(unsigned int x) {
    return x ? __builtin_ctz(x) + 1 : 0;
}

#define for_each_set_bit(bit, addr, size) \
    for ((bit) = find_first_bit((addr), (size)); \
         (bit) < (size); \
         (bit) = find_next_bit((addr), (size), (bit) + 1))

#define for_each_clear_bit(bit, addr, size) \
    for ((bit) = find_first_zero_bit((addr), (size)); \
         (bit) < (size); \
         (bit) = find_next_zero_bit((addr), (size), (bit) + 1))

static inline unsigned long bitmap_weight(const unsigned long *addr, unsigned long size) {
    unsigned long count = 0;
    for (unsigned long i = 0; i < BITS_TO_LONGS(size); i++)
        count += __builtin_popcount(addr[i]);
    return count;
}

#endif

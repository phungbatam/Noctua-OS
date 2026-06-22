#ifndef NOCTUA_ATOMIC_H
#define NOCTUA_ATOMIC_H

#include <stdint.h>

typedef struct {
    volatile int counter;
} atomic_t;

#define ATOMIC_INIT(i) { (i) }

static inline int atomic_read(const atomic_t *v) {
    return v->counter;
}

static inline void atomic_set(atomic_t *v, int i) {
    v->counter = i;
}

static inline void atomic_add(int i, atomic_t *v) {
    __asm__ volatile("lock addl %1, %0"
                     : "+m" (v->counter)
                     : "ir" (i)
                     : "memory");
}

static inline void atomic_sub(int i, atomic_t *v) {
    __asm__ volatile("lock subl %1, %0"
                     : "+m" (v->counter)
                     : "ir" (i)
                     : "memory");
}

static inline void atomic_inc(atomic_t *v) {
    __asm__ volatile("lock incl %0"
                     : "+m" (v->counter)
                     :
                     : "memory");
}

static inline void atomic_dec(atomic_t *v) {
    __asm__ volatile("lock decl %0"
                     : "+m" (v->counter)
                     :
                     : "memory");
}

static inline int atomic_inc_and_test(atomic_t *v) {
    unsigned char c;
    __asm__ volatile("lock incl %0; setz %1"
                     : "+m" (v->counter), "=qm" (c)
                     :
                     : "memory");
    return c;
}

static inline int atomic_dec_and_test(atomic_t *v) {
    unsigned char c;
    __asm__ volatile("lock decl %0; setz %1"
                     : "+m" (v->counter), "=qm" (c)
                     :
                     : "memory");
    return c;
}

static inline int atomic_add_negative(int i, atomic_t *v) {
    unsigned char c;
    __asm__ volatile("lock addl %2, %0; sets %1"
                     : "+m" (v->counter), "=qm" (c)
                     : "ir" (i)
                     : "memory");
    return c;
}

static inline int atomic_sub_and_test(int i, atomic_t *v) {
    unsigned char c;
    __asm__ volatile("lock subl %2, %0; setz %1"
                     : "+m" (v->counter), "=qm" (c)
                     : "ir" (i)
                     : "memory");
    return c;
}

static inline int atomic_cmpxchg(atomic_t *v, int old, int new) {
    int ret;
    __asm__ volatile("lock cmpxchgl %2, %1"
                     : "=a" (ret), "+m" (v->counter)
                     : "r" (new), "0" (old)
                     : "memory");
    return ret;
}

static inline int atomic_xchg(atomic_t *v, int new) {
    int ret;
    __asm__ volatile("lock xchgl %0, %1"
                     : "=r" (ret), "+m" (v->counter)
                     : "0" (new)
                     : "memory");
    return ret;
}

typedef struct {
    volatile long counter;
} atomic_long_t;

#define ATOMIC_LONG_INIT(i) { (i) }

static inline long atomic_long_read(const atomic_long_t *v) {
    return v->counter;
}

static inline void atomic_long_set(atomic_long_t *v, long i) {
    v->counter = i;
}

static inline void atomic_long_inc(atomic_long_t *v) {
    __asm__ volatile("lock addl %1, %0"
                     : "+m" (v->counter)
                     : "ir" (1)
                     : "memory");
}

static inline void atomic_long_dec(atomic_long_t *v) {
    __asm__ volatile("lock subl %1, %0"
                     : "+m" (v->counter)
                     : "ir" (1)
                     : "memory");
}

static inline long atomic_long_cmpxchg(atomic_long_t *v, long old, long new) {
    long ret;
    __asm__ volatile("lock cmpxchgl %2, %1"
                     : "=a" (ret), "+m" (v->counter)
                     : "r" (new), "0" (old)
                     : "memory");
    return ret;
}

#endif

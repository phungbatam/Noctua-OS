#ifndef NOCTUA_SPINLOCK_H
#define NOCTUA_SPINLOCK_H

#include <stdint.h>

typedef struct {
    volatile int locked;
} spinlock_t;

#define spin_lock_init(l)    do { (l)->locked = 0; } while (0)

static inline void spin_lock(spinlock_t *lock) {
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        while (lock->locked) __asm__ volatile("pause");
    }
}

static inline void spin_unlock(spinlock_t *lock) {
    __sync_lock_release(&lock->locked);
}

static inline void local_irq_save(uint32_t *flags) {
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(*flags) :: "memory");
}

static inline void local_irq_restore(uint32_t flags) {
    if (flags & 0x200)
        __asm__ volatile("sti" ::: "memory");
}

static inline void spin_lock_irqsave(spinlock_t *lock, uint32_t *flags) {
    local_irq_save(flags);
    spin_lock(lock);
}

static inline void spin_unlock_irqrestore(spinlock_t *lock, uint32_t flags) {
    spin_unlock(lock);
    local_irq_restore(flags);
}

typedef spinlock_t rwlock_t;
static inline void read_lock(rwlock_t *lock) { spin_lock(lock); }
static inline void read_unlock(rwlock_t *lock) { spin_unlock(lock); }
static inline void write_lock(rwlock_t *lock) { spin_lock(lock); }
static inline void write_unlock(rwlock_t *lock) { spin_unlock(lock); }

static inline int spin_is_locked(spinlock_t *lock) {
    return lock->locked;
}

#endif

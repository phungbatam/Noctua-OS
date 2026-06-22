#ifndef NOCTUA_HW_H
#define NOCTUA_HW_H

#include <stdint.h>

/* Atomic operations (use atomic.h instead for atomic_t type) */
int atomic_xchg_raw(volatile int *ptr, int val);
int atomic_cmpxchg_raw(volatile int *ptr, int old, int new);

/* Memory operations */
void *fast_memcpy(void *dest, const void *src, uint32_t len);
void *fast_memset(void *dest, int value, uint32_t len);

/* CPU operations */
void io_wait(void);
uint32_t read_tsc(void);
void halt_cpu(void);
void cli_custom(void);
void sti_custom(void);

#endif

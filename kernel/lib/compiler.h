#ifndef NOCTUA_COMPILER_H
#define NOCTUA_COMPILER_H

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define barrier()       __asm__ volatile("" ::: "memory")

#define __packed        __attribute__((__packed__))
#define __aligned(x)    __attribute__((__aligned__(x)))
#define __section(x)    __attribute__((__section__(x)))
#define __maybe_unused  __attribute__((__unused__))
#define __always_inline inline __attribute__((__always_inline__))
#define __weak          __attribute__((__weak__))
#define __noreturn      __attribute__((__noreturn__))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define ALIGN(x, a)     (((x) + (a) - 1) & ~((typeof(x))(a) - 1))
#define ALIGN_DOWN(x, a) ((x) & ~((typeof(x))(a) - 1))
#define IS_ALIGNED(x, a) (((x) & ((typeof(x))(a) - 1)) == 0)

#define min(x, y)       ({ \
    typeof(x) _x = (x); typeof(y) _y = (y); \
    _x < _y ? _x : _y; })

#define max(x, y)       ({ \
    typeof(x) _x = (x); typeof(y) _y = (y); \
    _x > _y ? _x : _y; })

#define clamp(val, lo, hi) min(max(val, lo), hi)

#define SWAP(a, b)      do { \
    typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; \
} while (0)



#define BUILD_BUG_ON(cond) ((void)sizeof(char[1 - 2 * !!(cond)]))
#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int:-!!(e); }))

#endif

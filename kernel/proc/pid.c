#include "pid.h"
#include "string.h"

#define PID_BITMAP_WORDS ((PID_MAX + 31) / 32)

static uint32_t pid_bitmap[PID_BITMAP_WORDS];

void pid_init(void) {
    memset(pid_bitmap, 0, sizeof(pid_bitmap));
    pid_bitmap[0] |= 1;
}

int pid_alloc(void) {
    for (int i = PID_MIN; i < PID_MAX; i++) {
        int word = i / 32;
        int bit = i % 32;
        if (!(pid_bitmap[word] & (1u << bit))) {
            pid_bitmap[word] |= (1u << bit);
            return i;
        }
    }
    return -1;
}

void pid_free(int pid) {
    if (pid < PID_MIN || pid >= PID_MAX) return;
    int word = pid / 32;
    int bit = pid % 32;
    pid_bitmap[word] &= ~(1u << bit);
}

int pid_is_used(int pid) {
    if (pid < PID_MIN || pid >= PID_MAX) return 0;
    int word = pid / 32;
    int bit = pid % 32;
    return (pid_bitmap[word] >> bit) & 1;
}

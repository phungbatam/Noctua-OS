#ifndef NOCTUA_COMPLETION_H
#define NOCTUA_COMPLETION_H

#include <stdint.h>
#include "wait.h"

typedef struct {
    unsigned int done;
    wait_queue_head_t wait;
} completion_t;

#define COMPLETION_INITIALIZER(work) \
    { 0, __WAIT_QUEUE_HEAD_INITIALIZER((work).wait) }

#define DECLARE_COMPLETION(work) \
    completion_t work = COMPLETION_INITIALIZER(work)

static inline void init_completion(completion_t *x) {
    x->done = 0;
    init_waitqueue_head(&x->wait);
}

static inline void reinit_completion(completion_t *x) {
    x->done = 0;
}

void wait_for_completion(completion_t *x);
int wait_for_completion_timeout(completion_t *x, unsigned long timeout);
void complete(completion_t *x);
void complete_all(completion_t *x);

#endif

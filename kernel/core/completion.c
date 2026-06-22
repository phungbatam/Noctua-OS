#include "completion.h"
#include "spinlock.h"
#include "proc/sched.h"

static spinlock_t completion_lock;

void complete(completion_t *x) {
    uint32_t flags;
    spin_lock_irqsave(&completion_lock, &flags);
    x->done++;
    spin_unlock_irqrestore(&completion_lock, flags);
    wake_up(&x->wait);
}

void complete_all(completion_t *x) {
    uint32_t flags;
    spin_lock_irqsave(&completion_lock, &flags);
    x->done = (unsigned int)-1;
    spin_unlock_irqrestore(&completion_lock, flags);
    wake_up(&x->wait);
}

void wait_for_completion(completion_t *x) {
    if (x->done) {
        x->done--;
        return;
    }

    while (!x->done) {
        wake_up(&x->wait);
        task_yield();
    }
    x->done--;
}

int wait_for_completion_timeout(completion_t *x, unsigned long timeout) {
    (void)timeout;
    if (x->done) {
        x->done--;
        return 1;
    }

    extern unsigned long jiffies(void);
    unsigned long start = jiffies();

    while (!x->done) {
        if (jiffies() - start >= timeout)
            return 0;
        wake_up(&x->wait);
        task_yield();
    }
    x->done--;
    return 1;
}

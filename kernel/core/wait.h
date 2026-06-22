#ifndef NOCTUA_WAIT_H
#define NOCTUA_WAIT_H

#include "list.h"
#include "proc/sched.h"

typedef struct {
    struct list_head task_list;
} wait_queue_head_t;

typedef int (*wait_queue_func_t)(void *wait, int mode, int flags, void *key);

typedef struct {
    struct list_head entry;
    void *task;
    wait_queue_func_t func;
} wait_queue_entry_t;

#define __WAITQUEUE_INITIALIZER(name, tsk) { \
    .entry = LIST_HEAD_INIT((name).entry),   \
    .task = tsk,                             \
    .func = NULL,                            \
}

#define DECLARE_WAITQUEUE(name, tsk) \
    wait_queue_entry_t name = __WAITQUEUE_INITIALIZER(name, tsk)

#define WAIT_FLAG_WOKEN 1

#define init_waitqueue_head(wq) INIT_LIST_HEAD(&(wq)->task_list)

void add_wait_queue(wait_queue_head_t *wq, wait_queue_entry_t *wq_entry);
void remove_wait_queue(wait_queue_head_t *wq, wait_queue_entry_t *wq_entry);
void __wake_up(wait_queue_head_t *wq, unsigned int mode, int nr, void *key);

#define wake_up(x) __wake_up(x, 0, 1, NULL)

int default_wake_function(void *wait, int mode, int flags, void *key);

#define wait_event(wq, condition) \
    do { \
        if (condition) break; \
        for (;;) { \
            wait_queue_entry_t __wait; \
            __wait.task = (void*)1; \
            __wait.func = NULL; \
            INIT_LIST_HEAD(&__wait.entry); \
            add_wait_queue(&wq, &__wait); \
            int __woken = 0; \
            while (!__woken && !(condition)) { \
                task_yield(); \
                __woken = (__wait.entry.next == NULL); \
            } \
            remove_wait_queue(&wq, &__wait); \
            break; \
        } \
    } while (0)

#define wait_event_interruptible(wq, condition) wait_event(wq, condition)

#endif

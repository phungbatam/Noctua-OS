#ifndef NOCTUA_WORKQUEUE_H
#define NOCTUA_WORKQUEUE_H

#include "list.h"

typedef void (*work_func_t)(void *data);

typedef struct {
    struct list_head entry;
    work_func_t func;
    void *data;
    int pending;
} work_t;

typedef struct {
    struct list_head head;
    int running;
} workqueue_t;

#define INIT_WORK(w, f, d) \
    do { \
        INIT_LIST_HEAD(&(w)->entry); \
        (w)->func = (f); \
        (w)->data = (d); \
        (w)->pending = 0; \
    } while (0)

workqueue_t *create_workqueue(const char *name);
int schedule_work(workqueue_t *wq, work_t *work);
void flush_workqueue(workqueue_t *wq);
void workqueue_thread(void);

extern workqueue_t system_wq;

#endif

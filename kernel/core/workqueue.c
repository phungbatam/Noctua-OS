#include "workqueue.h"
#include "printk.h"
#include "mm/heap.h"

workqueue_t system_wq;

workqueue_t *create_workqueue(const char *name) {
    (void)name;
    workqueue_t *wq = (workqueue_t *)kmalloc(sizeof(workqueue_t));
    if (!wq) return NULL;
    INIT_LIST_HEAD(&wq->head);
    wq->running = 1;
    return wq;
}

int schedule_work(workqueue_t *wq, work_t *work) {
    if (work->pending) return 0;
    work->pending = 1;
    list_add_tail(&work->entry, &wq->head);
    return 1;
}

void flush_workqueue(workqueue_t *wq) {
    while (!list_empty(&wq->head)) {
        workqueue_thread();
    }
}

void workqueue_thread(void) {
    if (list_empty(&system_wq.head)) return;

    work_t *work = list_first_entry(&system_wq.head, work_t, entry);
    list_del_init(&work->entry);
    work->pending = 0;
    if (work->func)
        work->func(work->data);
}

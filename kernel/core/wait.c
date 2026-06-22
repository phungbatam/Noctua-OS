#include "wait.h"

void add_wait_queue(wait_queue_head_t *wq, wait_queue_entry_t *wq_entry) {
    list_add(&wq_entry->entry, &wq->task_list);
}

void remove_wait_queue(wait_queue_head_t *wq, wait_queue_entry_t *wq_entry) {
    list_del(&wq_entry->entry);
    INIT_LIST_HEAD(&wq_entry->entry);
}

void __wake_up(wait_queue_head_t *wq, unsigned int mode, int nr, void *key) {
    (void)mode;
    (void)key;
    int cnt = nr;
    wait_queue_entry_t *pos, *tmp;
    list_for_each_entry_safe(pos, tmp, &wq->task_list, entry) {
        if (nr == 0 || cnt > 0) {
            list_del_init(&pos->entry);
            cnt--;
        }
    }
}

int default_wake_function(void *wait, int mode, int flags, void *key) {
    (void)wait;
    (void)mode;
    (void)flags;
    (void)key;
    return 0;
}

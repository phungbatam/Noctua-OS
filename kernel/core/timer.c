#include "timer.h"
#include "spinlock.h"
#include "printk.h"
#include "timer/pit.h"

static struct list_head timer_list_head;
static spinlock_t timer_lock;
static int timer_initialized = 0;

unsigned long jiffies(void) {
    return pit_get_ticks();
}

void timer_init(void) {
    INIT_LIST_HEAD(&timer_list_head);
    spin_lock_init(&timer_lock);
    pit_on_tick = timer_tick;
    timer_initialized = 1;
    printk("TIMER: Kernel timer subsystem initialized");
}

void add_timer(timer_list_t *timer) {
    uint32_t flags;
    if (!timer || timer->pending) return;

    spin_lock_irqsave(&timer_lock, &flags);
    timer->pending = 1;
    list_add_tail(&timer->entry, &timer_list_head);
    spin_unlock_irqrestore(&timer_lock, flags);
}

int mod_timer(timer_list_t *timer, unsigned long expires) {
    int was_pending = timer->pending;

    if (timer->pending)
        del_timer(timer);

    timer->expires = expires;
    add_timer(timer);
    return was_pending;
}

int del_timer(timer_list_t *timer) {
    uint32_t flags;
    if (!timer || !timer->pending) return 0;

    spin_lock_irqsave(&timer_lock, &flags);
    timer->pending = 0;
    list_del_init(&timer->entry);
    spin_unlock_irqrestore(&timer_lock, flags);
    return 1;
}

void timer_tick(void) {
    if (!timer_initialized) return;

    timer_list_t *pos, *tmp;
    list_for_each_entry_safe(pos, tmp, &timer_list_head, entry) {
        if (!pos->pending) continue;
        if (pos->expires <= jiffies()) {
            pos->pending = 0;
            list_del_init(&pos->entry);
            if (pos->callback)
                pos->callback(pos->data);
        }
    }
}

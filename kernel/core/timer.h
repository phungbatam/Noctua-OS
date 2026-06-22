#ifndef NOCTUA_TIMER_H
#define NOCTUA_TIMER_H

#include <stdint.h>
#include "list.h"

#define MAX_TIMER_CALLBACKS 64

typedef void (*timer_callback_t)(void *data);

typedef struct {
    struct list_head entry;
    unsigned long expires;
    timer_callback_t callback;
    void *data;
    int pending;
} timer_list_t;

#define TIMER_INITIALIZER(_callback, _data) { \
    .entry = LIST_HEAD_INIT((timer_list_t){0}.entry), \
    .expires = 0, \
    .callback = (_callback), \
    .data = (_data), \
    .pending = 0, \
}

static inline void timer_setup(timer_list_t *timer, timer_callback_t callback, void *data) {
    INIT_LIST_HEAD(&timer->entry);
    timer->expires = 0;
    timer->callback = callback;
    timer->data = data;
    timer->pending = 0;
}

static inline int timer_pending(const timer_list_t *timer) {
    return timer->pending;
}

void add_timer(timer_list_t *timer);
int mod_timer(timer_list_t *timer, unsigned long expires);
int del_timer(timer_list_t *timer);
void timer_tick(void);
void timer_init(void);
unsigned long jiffies(void);

#endif

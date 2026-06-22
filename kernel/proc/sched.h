#ifndef TVN_SCHED_H
#define TVN_SCHED_H

#include "task.h"

#define NICE_MIN     -20
#define NICE_MAX      19
#define NICE_DEFAULT   0
#define DEFAULT_TIMESLICE 5

struct registers;

void sched_init(void);
void sched_tick(void);
void sched_switch(struct registers *r);
task_t *sched_next(void);
void sched_add(task_t *t);
void sched_remove(task_t *t);
void sched_set_nice(task_t *t, int nice);

#endif

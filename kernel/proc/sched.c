#include "sched.h"
#include "isr.h"
#include "string.h"

static task_t *ready_queue = 0;
static int scheduler_active = 0;
static task_t *idle_task_ptr = 0;
static uint32_t total_ticks = 0;

extern task_t *current_task;
extern task_t task_pool[];
extern int task_count;

static uint32_t nice_to_weight(int nice) {
    int w = 1024 - nice * 40;
    return w > 0 ? (uint32_t)w : 1;
}

void sched_init(void) {
    ready_queue = 0;
    scheduler_active = 1;
    idle_task_ptr = task_get(0);
    total_ticks = 0;
}

task_t *sched_next(void) {
    if (!scheduler_active) return idle_task_ptr;
    if (!ready_queue) return idle_task_ptr;

    task_t *best = ready_queue;
    task_t *curr = ready_queue;
    do {
        if (curr->state == TASK_READY) {
            if (!best || best->state != TASK_READY) {
                best = curr;
            } else if (curr->vruntime < best->vruntime) {
                best = curr;
            }
        }
        curr = curr->next;
    } while (curr && curr != ready_queue);

    if (best && best->state != TASK_READY) {
        for (int i = 0; i < task_count; i++) {
            if (task_pool[i].state == TASK_READY) {
                best = &task_pool[i];
                break;
            }
        }
        if (best->state != TASK_READY) return idle_task_ptr;
    }

    return best && best->state == TASK_READY ? best : idle_task_ptr;
}

void sched_tick(void) {
    if (!scheduler_active) return;

    task_t *curr = current_task;
    if (curr && curr != idle_task_ptr) {
        curr->total_cpu_ticks++;
        curr->vruntime += 1024 * 10 / nice_to_weight(curr->nice);

        if (curr->timeslice > 0) {
            curr->timeslice--;
        }
    }
    total_ticks++;
}

static void regs_to_context(struct registers *r, context_t *c) {
    c->eax = r->eax;
    c->ebx = r->ebx;
    c->ecx = r->ecx;
    c->edx = r->edx;
    c->esi = r->esi;
    c->edi = r->edi;
    c->esp = r->esp;
    c->ebp = r->ebp;
    c->eip = r->eip;
    c->eflags = r->eflags;
    c->cr3 = 0;
}

static void context_to_regs(context_t *c, struct registers *r) {
    r->eax = c->eax;
    r->ebx = c->ebx;
    r->ecx = c->ecx;
    r->edx = c->edx;
    r->esi = c->esi;
    r->edi = c->edi;
    r->esp = c->esp;
    r->ebp = c->ebp;
    r->eip = c->eip;
    r->eflags = c->eflags;
}

void sched_switch(struct registers *r) {
    if (!scheduler_active) return;

    task_t *curr = current_task;
    if (curr && curr != idle_task_ptr) {
        regs_to_context(r, &curr->context);
        if (curr->timeslice == 0) {
            curr->timeslice = DEFAULT_TIMESLICE;
        }
    }

    task_t *next = sched_next();
    if (!next || next == curr) return;

    current_task = next;
    context_to_regs(&next->context, r);

    if (next->state == TASK_READY) next->state = TASK_RUNNING;
}

void sched_add(task_t *t) {
    if (!t) return;
    t->nice = NICE_DEFAULT;
    t->vruntime = total_ticks;
    t->timeslice = DEFAULT_TIMESLICE;

    if (!ready_queue) {
        ready_queue = t;
        t->next = t;
    } else {
        t->next = ready_queue->next;
        ready_queue->next = t;
    }
    t->state = TASK_READY;
}

void sched_remove(task_t *t) {
    if (!t || !ready_queue) return;

    task_t *curr = ready_queue;
    task_t *prev = 0;

    if (curr == t && curr->next == curr) {
        ready_queue = 0;
        t->next = 0;
        return;
    }

    if (curr == t) {
        while (curr->next != ready_queue) curr = curr->next;
        ready_queue = t->next;
        curr->next = ready_queue;
        t->next = 0;
        return;
    }

    prev = curr;
    curr = curr->next;
    while (curr && curr != ready_queue) {
        if (curr == t) {
            prev->next = curr->next;
            t->next = 0;
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

void sched_set_nice(task_t *t, int nice) {
    if (!t) return;
    if (nice < NICE_MIN) nice = NICE_MIN;
    if (nice > NICE_MAX) nice = NICE_MAX;
    t->nice = nice;
}

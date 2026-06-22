#include "task.h"
#include "pid.h"
#include "page.h"
#include "paging.h"
#include "string.h"
#include "sched.h"

task_t task_pool[MAX_TASKS];
int task_count = 0;
task_t *current_task = 0;
static task_t *idle_task = 0;

static void task_link_child(task_t *parent, task_t *child) {
    if (!parent || !child) return;
    child->parent = parent;
    child->sibling = parent->children;
    parent->children = child;
}

void task_init_system(void) {
    memset(task_pool, 0, sizeof(task_pool));
    task_count = 0;

    pid_init();

    /* Idle task - task 0 */
    idle_task = &task_pool[task_count++];
    idle_task->id = 0;
    idle_task->pid = 0;
    idle_task->pgid = 0;
    strcpy(idle_task->name, "idle");
    idle_task->state = TASK_READY;
    idle_task->priority = 0;
    idle_task->nice = 0;
    idle_task->vruntime = 0;
    idle_task->timeslice = 0;
    idle_task->total_cpu_ticks = 0;
    idle_task->kernel_stack = 0;
    idle_task->sig_pending = 0;
    idle_task->sig_blocked = 0;
    idle_task->parent = 0;
    idle_task->children = 0;
    idle_task->sibling = 0;
    for (int i = 0; i < NSIG; i++)
        idle_task->sig_handlers[i] = SIG_DFL;

    current_task = idle_task;
}

int task_create(const char *name, void (*entry)(void), uint32_t priority) {
    if (task_count >= MAX_TASKS) return -1;

    task_t *t = &task_pool[task_count];
    memset(t, 0, sizeof(task_t));

    t->id = pid_alloc();
    if (t->id < 0) {
        memset(t, 0, sizeof(task_t));
        return -1;
    }
    t->pid = t->id;
    t->pgid = t->pid;
    int i;
    for (i = 0; name[i] && i < TASK_NAME_MAX - 1; i++) t->name[i] = name[i];
    t->name[i] = 0;
    t->state = TASK_READY;
    t->priority = priority;
    t->nice = 0;
    t->vruntime = 0;
    t->timeslice = 5;
    t->total_cpu_ticks = 0;
    t->sig_pending = 0;
    t->sig_blocked = 0;
    for (i = 0; i < NSIG; i++)
        t->sig_handlers[i] = SIG_DFL;

    /* Cấp phát kernel stack */
    t->kernel_stack = (uint32_t *)pmem_alloc_page();
    if (!t->kernel_stack) {
        memset(t, 0, sizeof(task_t));
        return -1;
    }
    t->kernel_stack_size = PAGE_SIZE;

    /* Setup stack cho lần context-switch đầu tiên.
     * Layout phải giống như interrupt frame mà irq_common_stub để lại:
     *   [esp+0]  = err_code (dummy) -> add esp,8 skip
     *   [esp+4]  = int_no   (dummy) -> add esp,8 skip
     *   [esp+8]  = eip
     *   [esp+12] = cs (0x08 = kernel code segment)
     *   [esp+16] = eflags */
    uint32_t *stack = (uint32_t *)((uint32_t)t->kernel_stack + PAGE_SIZE);
    *--stack = 0x202;   /* eflags */
    *--stack = 0x08;    /* cs */
    *--stack = (uint32_t)entry; /* eip */
    *--stack = 0;       /* int_no dummy */
    *--stack = 0;       /* err_code dummy */
    t->context.esp = (uint32_t)stack;
    t->context.eip = (uint32_t)entry;
    t->context.eflags = 0x202;
    t->context.cr3 = 0;

    task_link_child(current_task, t);
    task_count++;
    return t->id;
}

int task_fork(void) {
    if (task_count >= MAX_TASKS) return -1;

    task_t *parent = current_task;
    if (!parent) return -1;

    task_t *child = &task_pool[task_count];
    memset(child, 0, sizeof(task_t));

    child->id = pid_alloc();
    if (child->id < 0) {
        pmem_free_page(child->kernel_stack);
        memset(child, 0, sizeof(task_t));
        return -1;
    }
    child->pid = child->id;
    child->pgid = parent->pgid;
    memcpy(child->name, parent->name, TASK_NAME_MAX);
    child->state = TASK_READY;
    child->priority = parent->priority;
    child->nice = parent->nice;
    child->vruntime = 0;
    child->timeslice = 5;
    child->total_cpu_ticks = 0;
    child->exit_code = 0;
    child->sig_pending = 0;
    child->sig_blocked = parent->sig_blocked;
    for (int i = 0; i < NSIG; i++)
        child->sig_handlers[i] = parent->sig_handlers[i];

    /* Cấp phát kernel stack mới và copy stack content */
    child->kernel_stack = (uint32_t *)pmem_alloc_page();
    if (!child->kernel_stack) {
        memset(child, 0, sizeof(task_t));
        return -1;
    }
    child->kernel_stack_size = PAGE_SIZE;

    uint32_t stack_bottom = (uint32_t)parent->kernel_stack;
    uint32_t stack_top = stack_bottom + PAGE_SIZE;
    uint32_t parent_esp = parent->context.esp;
    uint32_t stack_used = stack_top - parent_esp;

    if (stack_used > 0 && stack_used <= PAGE_SIZE) {
        memcpy(
            (void *)((uint32_t)child->kernel_stack + PAGE_SIZE - stack_used),
            (void *)parent_esp,
            stack_used
        );
    }

    child->context = parent->context;
    child->context.esp = (uint32_t)child->kernel_stack + PAGE_SIZE - stack_used;
    child->context.eax = 0; /* fork trả về 0 trong child */

    task_link_child(parent, child);
    sched_add(child);
    task_count++;

    return child->pid;
}

void task_exit(int code) {
    task_t *t = current_task;
    if (!t) return;
    pid_free(t->pid);
    t->state = TASK_ZOMBIE;
    t->exit_code = code;
    /* Báo hiệu cho parent (nếu đang WAITING) */
    task_t *par = t->parent;
    if (par && par->state == TASK_WAITING) {
        par->state = TASK_READY;
    }
    task_yield();
}

void task_yield(void) {
    asm volatile("int $0x20");
}

void task_block(void) {
    task_t *t = current_task;
    if (t) t->state = TASK_BLOCKED;
    task_yield();
}

void task_unblock(int task_id) {
    for (int i = 0; i < task_count; i++) {
        if (task_pool[i].id == task_id && (task_pool[i].state == TASK_BLOCKED || task_pool[i].state == TASK_WAITING)) {
            task_pool[i].state = TASK_READY;
            return;
        }
    }
}

task_t *task_current(void) { return current_task; }

task_t *task_get(int id) {
    for (int i = 0; i < task_count; i++) {
        if (task_pool[i].id == id) return &task_pool[i];
    }
    return 0;
}

task_t *task_find_by_pid(int pid) {
    for (int i = 0; i < task_count; i++) {
        if (task_pool[i].pid == pid) return &task_pool[i];
    }
    return 0;
}

void task_set_state(int id, task_state_t state) {
    task_t *t = task_get(id);
    if (t) t->state = state;
}

void task_set_pgid(int pid, int pgid) {
    task_t *t = task_find_by_pid(pid);
    if (t) t->pgid = pgid;
}
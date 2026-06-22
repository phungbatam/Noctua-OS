#ifndef TVN_TASK_H
#define TVN_TASK_H

#include <stdint.h>

#define MAX_TASKS      32
#define TASK_STACK_SIZE PAGE_SIZE
#define TASK_NAME_MAX  32

/* Signal numbers */
#define SIGKILL   1
#define SIGINT    2
#define SIGSEGV   3
#define SIGPIPE   4
#define SIGTERM   5
#define NSIG      16

/* Signal actions */
#define SIG_DFL ((void *)0)
#define SIG_IGN ((void *)1)

/* Trạng thái task - khác Linux: TVN dùng kiểu đơn giản hơn */
typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_WAITING,
    TASK_ZOMBIE
} task_state_t;

/* Context lưu registers cho context switch */
typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, esp, ebp;
    uint32_t eip, eflags;
    uint32_t cr3;
    uint32_t useresp;
    uint32_t ss;
    uint32_t cs;
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;
} context_t;

/* Task Control Block - TVN_OS style */
/* Đơn giản hơn task_struct của Linux rất nhiều */
typedef struct task {
    int id;
    int pid;
    int pgid;                  /* process group ID */
    char name[TASK_NAME_MAX];
    task_state_t state;
    uint32_t priority;
    int32_t  nice;                  /* -20 (highest) to +19 (lowest) */
    uint32_t vruntime;              /* virtual runtime for CFS-like */
    uint32_t timeslice;             /* remaining ticks before preempt */
    context_t context;
    uint32_t *kernel_stack;
    uint32_t kernel_stack_size;
    uint32_t total_cpu_ticks;
    int exit_code;
    uint32_t sig_pending;      /* bitmask: signal dang cho */
    uint32_t sig_blocked;      /* bitmask: signal bi chan */
    void (*sig_handlers[NSIG])(int); /* handler per signal */
    struct task *parent;       /* task cha */
    struct task *children;     /* linked list con */
    struct task *sibling;      /* task anh em cung cha */
    struct task *next;
} task_t;

/* Task hiện tại (global, defined in task.c) */
extern struct task *current_task;

/* Task API */
void task_init_system(void);
int task_create(const char *name, void (*entry)(void), uint32_t priority);
int task_fork(void);  /* fork: copy current_task, child gets eax=0 */
void task_exit(int code);
void task_yield(void);
void task_block(void);
void task_unblock(int task_id);
task_t *task_current(void);
task_t *task_get(int id);
task_t *task_find_by_pid(int pid);
void task_set_state(int id, task_state_t state);
void task_set_pgid(int pid, int pgid);

#endif
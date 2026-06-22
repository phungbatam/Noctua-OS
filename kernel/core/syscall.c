#include "syscall.h"
#include "isr.h"
#include "screen.h"
#include "proc/task.h"
#include "proc/sched.h"
#include "fs/fat32.h"
#include "mm/heap.h"
#include "mm/page.h"
#include "timer/pit.h"
#include "input/keyboard.h"
#include "lib/string.h"
#include "core/elf.h"
#include "ipc/shm.h"
#include "ipc/msg.h"

/* Forward declarations from proc/task.c */
extern int task_fork(void);

/* ---- File descriptor table (global, per-task later) ---- */

#define MAX_FDS 64

typedef struct {
    int    is_open;        /* 1 = dang mo */
    int    is_pipe;        /* 1 = pipe endpoint */
    int    pipe_id;        /* id cua pipe (neu is_pipe) */
    int    is_pipe_write;  /* 1 = write end, 0 = read end */
    vfs_node_t *vnode;     /* VFS node (neu ko phai pipe) */
    uint32_t pos;          /* offset doc/ghi */
    int    flags;          /* O_RDONLY, O_WRONLY, ... */
} fd_entry_t;

static fd_entry_t fd_table[MAX_FDS];
static int pipe_next_id = 1;

/* ---- Pipe data ---- */

typedef struct {
    int id;
    int refs;          /* so fd dang tham chieu */
    uint8_t buf[4096];
    uint32_t head;
    uint32_t tail;
    int readers;
    int writers;
} pipe_t;

#define MAX_PIPES 16
static pipe_t pipes[MAX_PIPES];
static int pipe_count = 0;

void syscall_init(void) {
    for (int i = 0; i < MAX_FDS; i++)
        fd_table[i].is_open = 0;

    /* Stdin: fd 0 */
    fd_table[0].is_open = 1;
    fd_table[0].is_pipe = 0;
    fd_table[0].vnode = 0;
    fd_table[0].pos = 0;
    fd_table[0].flags = 0;

    /* Stdout: fd 1 */
    fd_table[1].is_open = 1;
    fd_table[1].is_pipe = 0;
    fd_table[1].vnode = 0;
    fd_table[1].pos = 0;
    fd_table[1].flags = 0;

    /* Stderr: fd 2 */
    fd_table[2].is_open = 1;
    fd_table[2].is_pipe = 0;
    fd_table[2].vnode = 0;
    fd_table[2].pos = 0;
    fd_table[2].flags = 0;
}

static int fd_alloc(void) {
    for (int i = 3; i < MAX_FDS; i++) {
        if (!fd_table[i].is_open) {
            fd_table[i].is_open = 1;
            return i;
        }
    }
    return -1;
}

static fd_entry_t *fd_get(int fd) {
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].is_open)
        return 0;
    return &fd_table[fd];
}

/* ---- Pipe internals ---- */

static pipe_t *pipe_create(void) {
    if (pipe_count >= MAX_PIPES) return 0;
    pipe_t *p = &pipes[pipe_count++];
    p->id = pipe_next_id++;
    p->refs = 0;
    p->head = 0;
    p->tail = 0;
    p->readers = 0;
    p->writers = 0;
    return p;
}

static pipe_t *pipe_find(int id) {
    for (int i = 0; i < pipe_count; i++) {
        if (pipes[i].id == id) return &pipes[i];
    }
    return 0;
}

static int pipe_read_bytes(pipe_t *p, uint8_t *buf, uint32_t count) {
    uint32_t read = 0;
    while (read < count && p->tail != p->head) {
        buf[read++] = p->buf[p->tail % 4096];
        p->tail++;
    }
    return read;
}

static int pipe_write_bytes(pipe_t *p, const uint8_t *buf, uint32_t count) {
    uint32_t written = 0;
    while (written < count && (p->head - p->tail) < 4096) {
        p->buf[p->head % 4096] = buf[written++];
        p->head++;
    }
    return written;
}

/* ---- Syscall dispatch ---- */

void syscall_handler(struct registers *r) {
    uint32_t n = r->eax;
    uint32_t a = r->ebx;
    uint32_t b = r->ecx;
    uint32_t c = r->edx;

    switch (n) {
    case SYS_OPEN:    r->eax = sys_open((const char *)a, (int)b);      break;
    case SYS_READ:    r->eax = sys_read((int)a, (void *)b, c);         break;
    case SYS_WRITE:   r->eax = sys_write((int)a, (const void *)b, c);  break;
    case SYS_CLOSE:   r->eax = (uint32_t)sys_close((int)a);            break;
    case SYS_GETPID:  r->eax = (uint32_t)sys_getpid();                 break;
    case SYS_EXIT:    sys_exit((int)a);                                 break;
    case SYS_SBRK:    r->eax = (uint32_t)sys_sbrk((intptr_t)a);        break;
    case SYS_PIPE:    r->eax = (uint32_t)sys_pipe((int *)a);           break;
    case SYS_SLEEP:   sys_sleep(a);                                     break;
    case SYS_GETTICK: r->eax = sys_get_tick();                         break;
    case SYS_FORK:
        /* Save syscall registers as parent context, then fork */
        current_task->context.eax = r->eax;
        current_task->context.ebx = r->ebx;
        current_task->context.ecx = r->ecx;
        current_task->context.edx = r->edx;
        current_task->context.esi = r->esi;
        current_task->context.edi = r->edi;
        current_task->context.esp = r->esp;
        current_task->context.ebp = r->ebp;
        current_task->context.eip = r->eip;
        current_task->context.eflags = r->eflags;
        r->eax = (uint32_t)sys_fork();
        break;
    case SYS_WAITPID: r->eax = (uint32_t)sys_waitpid((int)a, (int *)b, (int)c); break;
    case SYS_GETPPID: r->eax = (uint32_t)sys_getppid();                 break;
    case SYS_GETPGID: r->eax = (uint32_t)sys_getpgid((int)a);          break;
    case SYS_SETPGID: r->eax = (uint32_t)sys_setpgid((int)a, (int)b);  break;
    case SYS_EXECVE:  r->eax = (uint32_t)sys_execve((const char *)a, (char *const *)b, (char *const *)c); break;
    case SYS_STAT:    r->eax = (uint32_t)sys_stat((const char *)a, (void *)b); break;
    case SYS_FSTAT:   r->eax = (uint32_t)sys_fstat((int)a, (void *)b); break;
    case SYS_LSEEK:   r->eax = sys_lseek((int)a, b, (int)c); break;
    case SYS_DUP:     r->eax = (uint32_t)sys_dup((int)a); break;
    case SYS_DUP2:    r->eax = (uint32_t)sys_dup2((int)a, (int)b); break;
    case SYS_SHMGET:  r->eax = (uint32_t)sys_shmget((int)a, b, (int)c); break;
    case SYS_SHMAT:   r->eax = (uint32_t)sys_shmat((int)a, (const void *)b, (int)c); break;
    case SYS_SHMDT:   r->eax = (uint32_t)sys_shmdt((const void *)a); break;
    case SYS_SHMCTL:  r->eax = (uint32_t)sys_shmctl((int)a, (int)b, (void *)c); break;
    case SYS_MSGGET:  r->eax = (uint32_t)sys_msgget((int)a, (int)b); break;
    case SYS_MSGSND:  r->eax = (uint32_t)sys_msgsnd((int)a, (const void *)b, c, 0); break;
    case SYS_MSGRCV:  r->eax = (uint32_t)sys_msgrcv((int)a, (void *)b, c, 0, 0); break;
    case SYS_MSGCTL:  r->eax = (uint32_t)sys_msgctl((int)a, (int)b, (void *)c); break;
    case SYS_SIGACTION: r->eax = (uint32_t)sys_sigaction((int)a, (const void *)b, (void *)c); break;
    case SYS_SIGPROCMASK: r->eax = (uint32_t)sys_sigprocmask((int)a, (const void *)b, (void *)c); break;
    case SYS_KILL:    r->eax = (uint32_t)sys_kill((int)a, (int)b); break;
    case SYS_SETPRIORITY: r->eax = (uint32_t)sys_setpriority((int)a, (int)b, (int)c); break;
    case SYS_GETPRIORITY: r->eax = (uint32_t)sys_getpriority((int)a, (int)b); break;
    default:          r->eax = 0xFFFFFFFF;                              break;
    }
}

/* ---- Syscall implementations ---- */

uint32_t sys_open(const char *path, int flags) {
    if (!path) return 0xFFFFFFFF;

    int fd = fd_alloc();
    if (fd < 0) return 0xFFFFFFFF;

    /* Tim VFS node */
    vfs_node_t *node = vfs_lookup(path);
    if (!node) {
        fd_table[fd].is_open = 0;
        return 0xFFFFFFFF;
    }

    fd_table[fd].vnode = node;
    fd_table[fd].is_pipe = 0;
    fd_table[fd].pos = 0;
    fd_table[fd].flags = flags;

    return (uint32_t)fd;
}

uint32_t sys_read(int fd, void *buf, uint32_t count) {
    fd_entry_t *f = fd_get(fd);
    if (!f || !buf || !count) return 0xFFFFFFFF;

    if (f->is_pipe) {
        pipe_t *p = pipe_find(f->pipe_id);
        if (!p || f->is_pipe_write) return 0xFFFFFFFF;
        return (uint32_t)pipe_read_bytes(p, (uint8_t *)buf, count);
    }

    if (fd == 0) {
        /* Stdin - doc tu buffer ban phim */
        uint8_t *b = (uint8_t *)buf;
        for (uint32_t i = 0; i < count; i++) {
            int c = keyboard_getchar();
            if (c < 0) break;
            b[i] = (uint8_t)c;
        }
        return count;
    }

    if (!f->vnode) return 0xFFFFFFFF;

    if (f->vnode->f_op && f->vnode->f_op->read) {
        int ret = f->vnode->f_op->read(f->vnode, f->pos, count, buf);
        if (ret > 0) f->pos += ret;
        return ret;
    }

    return 0xFFFFFFFF;
}

uint32_t sys_write(int fd, const void *buf, uint32_t count) {
    fd_entry_t *f = fd_get(fd);
    if (!f || !buf || !count) return 0xFFFFFFFF;

    if (f->is_pipe) {
        pipe_t *p = pipe_find(f->pipe_id);
        if (!p || !f->is_pipe_write) return 0xFFFFFFFF;
        return (uint32_t)pipe_write_bytes(p, (const uint8_t *)buf, count);
    }

    /* VFS node write */
    if (f->vnode && f->vnode->f_op && f->vnode->f_op->write) {
        int ret = f->vnode->f_op->write(f->vnode, f->pos, count, buf);
        if (ret > 0) f->pos += ret;
        return ret;
    }

    /* stdout/stderr: ghi ra man hinh */
    if (fd == 1 || fd == 2) {
        const char *s = (const char *)buf;
        screen_term_write_buf(s, count);
        return count;
    }

    return 0xFFFFFFFF;
}

int sys_close(int fd) {
    fd_entry_t *f = fd_get(fd);
    if (!f) return -1;

    if (f->is_pipe) {
        pipe_t *p = pipe_find(f->pipe_id);
        if (p) {
            p->refs--;
            if (f->is_pipe_write) p->writers--;
            else p->readers--;
        }
    }

    f->is_open = 0;
    return 0;
}

int sys_getpid(void) {
    return task_current() ? task_current()->pid : 0;
}

void sys_exit(int status) {
    task_t *t = task_current();
    if (t) {
        t->state = TASK_ZOMBIE;
        t->exit_code = status;
    }
    task_yield();
    for (;;);
}

void *sys_sbrk(intptr_t increment) {
    (void)increment;
    return 0;
}

int sys_pipe(int *pipefd) {
    if (!pipefd) return -1;

    pipe_t *p = pipe_create();
    if (!p) return -1;

    int rfd = fd_alloc();
    int wfd = fd_alloc();
    if (rfd < 0 || wfd < 0) return -1;

    p->refs = 2;
    p->readers = 1;
    p->writers = 1;

    fd_table[rfd].is_pipe = 1;
    fd_table[rfd].pipe_id = p->id;
    fd_table[rfd].is_pipe_write = 0;

    fd_table[wfd].is_pipe = 1;
    fd_table[wfd].pipe_id = p->id;
    fd_table[wfd].is_pipe_write = 1;

    pipefd[0] = rfd;
    pipefd[1] = wfd;
    return 0;
}

void sys_sleep(uint32_t ms) {
    pit_sleep(ms);
}

uint32_t sys_get_tick(void) {
    return pit_get_ticks();
}

int sys_fork(void) {
    return task_fork();
}

int sys_waitpid(int pid, int *status, int options) {
    (void)options;
    task_t *curr = task_current();
    if (!curr) return -1;

    for (;;) {
        /* Duyệt danh sách con */
        task_t *child = curr->children;
        while (child) {
            if (pid <= 0 || child->pid == pid) {
                if (child->state == TASK_ZOMBIE) {
                    int exit_code = child->exit_code;
                    if (status) *status = exit_code;
                    return child->pid;
                }
            }
            child = child->sibling;
        }

        /* Nếu pid chỉ định cụ thể mà không tìm thấy */
        if (pid > 0) {
            task_t *target = task_find_by_pid(pid);
            if (!target || target->parent != curr) {
                return -1; /* Không có child nào với pid này */
            }
        }

        /* Nếu WNOHANG, trả về 0 */
        if (options & 1) return 0;

        /* Block đến khi child chết */
        curr->state = TASK_WAITING;
        task_yield();
    }
}

int sys_getppid(void) {
    task_t *curr = task_current();
    if (!curr || !curr->parent) return 0;
    return curr->parent->pid;
}

int sys_getpgid(int pid) {
    if (pid == 0) {
        task_t *curr = task_current();
        return curr ? curr->pgid : 0;
    }
    task_t *t = task_find_by_pid(pid);
    return t ? t->pgid : -1;
}

int sys_setpgid(int pid, int pgid) {
    if (pid == 0) pid = task_current()->pid;
    if (pgid == 0) pgid = pid;
    task_set_pgid(pid, pgid);
    return 0;
}

/* ---- New syscalls for real OS functionality ---- */

/* Stat structure for file information */
typedef struct {
    uint32_t st_dev;
    uint32_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t st_rdev;
    uint32_t st_size;
    uint32_t st_blksize;
    uint32_t st_blocks;
    uint32_t st_atime;
    uint32_t st_mtime;
    uint32_t st_ctime;
} stat_t;

/* File type flags */
#define S_IFDIR  0040000
#define S_IFREG  0100000
#define S_IFMT   0170000

int sys_execve(const char *path, char *const argv[], char *const envp[]) {
    (void)argv;
    (void)envp;
    
    if (!path) return -1;
    
    /* Find the file in VFS */
    vfs_node_t *node = vfs_lookup(path);
    if (!node) return -1;
    
    /* Load ELF and replace current process */
    elf_load_result_t result = elf_load(node);
    if (!result.success) return -1;
    
    /* Update current task context to jump to new entry point */
    task_t *t = task_current();
    if (t) {
        t->context.eip = result.entry_point;
        /* Set up new stack */
        uint32_t *stack = (uint32_t *)pmem_alloc_page();
        if (stack) {
            uint32_t stack_top = (uint32_t)stack + PAGE_SIZE - 16;
            t->context.esp = stack_top;
            t->context.eflags = 0x202;
        }
    }
    
    /* Never returns on success */
    return 0;
}

int sys_stat(const char *path, void *statbuf) {
    if (!path || !statbuf) return -1;
    
    vfs_node_t *node = vfs_lookup(path);
    if (!node) return -1;
    
    stat_t *st = (stat_t *)statbuf;
    memset(st, 0, sizeof(stat_t));
    
    st->st_size = node->size;
    st->st_mode = node->is_directory ? S_IFDIR : S_IFREG;
    st->st_ino = (uint32_t)node; /* Use pointer as inode number */
    
    return 0;
}

int sys_fstat(int fd, void *statbuf) {
    if (!statbuf) return -1;
    
    fd_entry_t *f = fd_get(fd);
    if (!f) return -1;
    
    stat_t *st = (stat_t *)statbuf;
    memset(st, 0, sizeof(stat_t));
    
    if (f->vnode) {
        st->st_size = f->vnode->size;
        st->st_mode = f->vnode->is_directory ? S_IFDIR : S_IFREG;
        st->st_ino = (uint32_t)f->vnode; /* Use pointer as inode number */
    }
    
    return 0;
}

uint32_t sys_lseek(int fd, uint32_t offset, int whence) {
    fd_entry_t *f = fd_get(fd);
    if (!f) return 0xFFFFFFFF;
    
    uint32_t new_pos;
    switch (whence) {
        case 0: /* SEEK_SET */
            new_pos = offset;
            break;
        case 1: /* SEEK_CUR */
            new_pos = f->pos + offset;
            break;
        case 2: /* SEEK_END */
            if (f->vnode) {
                new_pos = f->vnode->size + offset;
            } else {
                new_pos = offset;
            }
            break;
        default:
            return 0xFFFFFFFF;
    }
    
    f->pos = new_pos;
    return new_pos;
}

int sys_dup(int oldfd) {
    return sys_dup2(oldfd, -1);
}

int sys_dup2(int oldfd, int newfd) {
    fd_entry_t *old = fd_get(oldfd);
    if (!old) return -1;
    
    if (newfd < 0) {
        /* Allocate new fd */
        newfd = fd_alloc();
        if (newfd < 0) return -1;
    } else {
        if (newfd >= MAX_FDS) return -1;
        /* Close existing fd if open */
        if (fd_table[newfd].is_open) {
            sys_close(newfd);
        }
    }
    
    /* Copy fd entry */
    fd_table[newfd] = *old;
    fd_table[newfd].is_open = 1;
    
    return newfd;
}

/* ---- Shared memory syscalls ---- */

int sys_shmget(int key, uint32_t size, int flags) {
    (void)flags;
    return shm_create(key, size);
}

void *sys_shmat(int shmid, const void *shmaddr, int shmflg) {
    (void)shmaddr;
    (void)shmflg;
    return shm_attach(shmid);
}

int sys_shmdt(const void *shmaddr) {
    return shm_detach((void *)shmaddr);
}

int sys_shmctl(int shmid, int cmd, void *buf) {
    (void)buf;
    switch (cmd) {
        case 1: /* IPC_RMID - remove segment */
            return shm_delete(shmid);
        default:
            return -1;
    }
}

/* ---- Message queue syscalls ---- */

int sys_msgget(int key, int msgflg) {
    return msg_get(key, msgflg);
}

int sys_msgsnd(int msqid, const void *msgp, uint32_t msgsz, int msgflg) {
    return msg_send(msqid, msgp, msgsz, msgflg);
}

int sys_msgrcv(int msqid, void *msgp, uint32_t msgsz, int msgtyp, int msgflg) {
    (void)msgtyp;
    return msg_recv(msqid, msgp, msgsz, msgflg);
}

int sys_msgctl(int msqid, int cmd, void *buf) {
    return msg_ctl(msqid, cmd, buf);
}

/* ---- Signal syscalls ---- */

typedef struct {
    void (*sa_handler)(int);
    void (*sa_sigaction)(int, void *, void *);
    uint32_t sa_mask;
    int sa_flags;
    void (*sa_restorer)(void);
} sigaction_t;

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

int sys_sigaction(int signum, const void *act, void *oldact) {
    task_t *t = task_current();
    if (!t) return -1;
    
    if (signum < 1 || signum >= NSIG) return -1;
    
    if (oldact) {
        /* Save old handler */
        memcpy(oldact, &t->sig_handlers[signum], sizeof(sigaction_t));
    }
    
    if (act) {
        /* Set new handler */
        memcpy(&t->sig_handlers[signum], act, sizeof(sigaction_t));
    }
    
    return 0;
}

int sys_sigprocmask(int how, const void *set, void *oldset) {
    task_t *t = task_current();
    if (!t) return -1;
    
    if (oldset) {
        *(uint32_t *)oldset = t->sig_blocked;
    }
    
    if (set) {
        uint32_t mask = *(uint32_t *)set;
        switch (how) {
            case SIG_BLOCK:
                t->sig_blocked |= mask;
                break;
            case SIG_UNBLOCK:
                t->sig_blocked &= ~mask;
                break;
            case SIG_SETMASK:
                t->sig_blocked = mask;
                break;
            default:
                return -1;
        }
    }
    
    return 0;
}

int sys_kill(int pid, int sig) {
    if (sig < 1 || sig >= NSIG) return -1;
    
    task_t *target = task_find_by_pid(pid);
    if (!target) return -1;
    
    /* Set signal pending */
    target->sig_pending |= (1 << (sig - 1));
    
    return 0;
}

int sys_setpriority(int which, int who, int prio) {
    (void)which;
    (void)who;
    
    task_t *t = task_current();
    if (!t) return -1;
    
    /* Clamp priority between -20 and 19 */
    if (prio < -20) prio = -20;
    if (prio > 19) prio = 19;
    
    t->nice = prio;
    return 0;
}

int sys_getpriority(int which, int who) {
    (void)which;
    (void)who;
    
    task_t *t = task_current();
    if (!t) return -1;
    
    return t->nice;
}

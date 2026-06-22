#ifndef TVN_SYSCALL_H
#define TVN_SYSCALL_H

#include <stdint.h>
#include "isr.h"

/* Syscall numbers */
#define SYS_OPEN    1
#define SYS_READ    2
#define SYS_WRITE   3
#define SYS_CLOSE   4
#define SYS_GETPID  5
#define SYS_EXIT    6
#define SYS_SBRK    7
#define SYS_PIPE    8
#define SYS_SLEEP   9
#define SYS_GETTICK 10
#define SYS_FORK    11
#define SYS_WAITPID 12
#define SYS_GETPPID 13
#define SYS_GETPGID 14
#define SYS_SETPGID 15
#define SYS_EXECVE  16
#define SYS_STAT    17
#define SYS_FSTAT   18
#define SYS_LSEEK   19
#define SYS_DUP     20
#define SYS_DUP2    21
#define SYS_SHMGET  22
#define SYS_SHMAT   23
#define SYS_SHMDT   24
#define SYS_SHMCTL  25
#define SYS_MSGGET  26
#define SYS_MSGSND  27
#define SYS_MSGRCV  28
#define SYS_MSGCTL  29
#define SYS_SIGACTION 30
#define SYS_SIGPROCMASK 31
#define SYS_KILL 32
#define SYS_SETPRIORITY 33
#define SYS_GETPRIORITY 34

/* File descriptor numbers */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* Pipe direction */
#define PIPE_READ  0
#define PIPE_WRITE 1

#ifndef __ASSEMBLER__

/* Syscall dispatcher */
void syscall_init(void);
void syscall_handler(struct registers *r);

/* Syscall implementations */
uint32_t sys_open(const char *path, int flags);
uint32_t sys_read(int fd, void *buf, uint32_t count);
uint32_t sys_write(int fd, const void *buf, uint32_t count);
int      sys_close(int fd);
int      sys_getpid(void);
void     sys_exit(int status);
void    *sys_sbrk(intptr_t increment);
int      sys_pipe(int *pipefd);
void     sys_sleep(uint32_t ms);
uint32_t sys_get_tick(void);
int      sys_fork(void);
int      sys_waitpid(int pid, int *status, int options);
int      sys_getppid(void);
int      sys_getpgid(int pid);
int      sys_setpgid(int pid, int pgid);
int      sys_execve(const char *path, char *const argv[], char *const envp[]);
int      sys_stat(const char *path, void *statbuf);
int      sys_fstat(int fd, void *statbuf);
uint32_t sys_lseek(int fd, uint32_t offset, int whence);
int      sys_dup(int oldfd);
int      sys_dup2(int oldfd, int newfd);
int      sys_shmget(int key, uint32_t size, int flags);
void    *sys_shmat(int shmid, const void *shmaddr, int shmflg);
int      sys_shmdt(const void *shmaddr);
int      sys_shmctl(int shmid, int cmd, void *buf);
int      sys_msgget(int key, int msgflg);
int      sys_msgsnd(int msqid, const void *msgp, uint32_t msgsz, int msgflg);
int      sys_msgrcv(int msqid, void *msgp, uint32_t msgsz, int msgtyp, int msgflg);
int      sys_msgctl(int msqid, int cmd, void *buf);
int      sys_sigaction(int signum, const void *act, void *oldact);
int      sys_sigprocmask(int how, const void *set, void *oldset);
int      sys_kill(int pid, int sig);
int      sys_setpriority(int which, int who, int prio);
int      sys_getpriority(int which, int who);

#endif

#endif

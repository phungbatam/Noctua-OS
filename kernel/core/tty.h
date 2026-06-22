#ifndef TVN_TTY_H
#define TVN_TTY_H

#include <stdint.h>

#define TTY_BUF_SIZE 4096
#define TTY_NUM 4

typedef struct {
    int active;
    char input_buffer[TTY_BUF_SIZE];
    int input_head;
    int input_tail;
    int echo;
    int line_buffer;
    int pid_foreground;
    uint32_t flags;
} tty_t;

/* TTY flags */
#define TTY_ACTIVE    0x01
#define TTY_ECHO      0x02
#define TTY_LINE_BUF  0x04

int  tty_init(void);
int  tty_open(int tty_num);
int  tty_close(int tty_num);
int  tty_read(int tty_num, char *buf, int count);
int  tty_write(int tty_num, const char *buf, int count);
int  tty_putchar(int tty_num, char c);
void tty_input(int tty_num, char c);
int  tty_set_foreground(int tty_num, int pid);
int  tty_get_foreground(int tty_num);

#endif

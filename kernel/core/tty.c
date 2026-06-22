#include "tty.h"
#include "screen.h"
#include "string.h"

static tty_t ttys[TTY_NUM];
static int active_tty = 0;

int tty_init(void) {
    memset(ttys, 0, sizeof(ttys));
    for (int i = 0; i < TTY_NUM; i++) {
        ttys[i].active = 1;
        ttys[i].echo = 1;
        ttys[i].line_buffer = 1;
        ttys[i].input_head = 0;
        ttys[i].input_tail = 0;
        ttys[i].pid_foreground = -1;
        ttys[i].flags = TTY_ACTIVE | TTY_ECHO | TTY_LINE_BUF;
    }
    active_tty = 0;
    return 0;
}

int tty_open(int tty_num) {
    if (tty_num < 0 || tty_num >= TTY_NUM) return -1;
    ttys[tty_num].active = 1;
    return 0;
}

int tty_close(int tty_num) {
    if (tty_num < 0 || tty_num >= TTY_NUM) return -1;
    ttys[tty_num].active = 0;
    return 0;
}

int tty_read(int tty_num, char *buf, int count) {
    if (tty_num < 0 || tty_num >= TTY_NUM || !buf || !ttys[tty_num].active)
        return -1;

    tty_t *tty = &ttys[tty_num];
    int bytes_read = 0;

    while (bytes_read < count) {
        if (tty->input_head != tty->input_tail) {
            char c = tty->input_buffer[tty->input_tail];
            tty->input_tail = (tty->input_tail + 1) % TTY_BUF_SIZE;

            if (tty->line_buffer && c == '\n') {
                buf[bytes_read++] = c;
                return bytes_read;
            }

            buf[bytes_read++] = c;
        } else {
            if (bytes_read > 0) return bytes_read;
            return 0;
        }
    }

    return bytes_read;
}

static void tty_echo(int tty_num, unsigned char c) {
    (void)tty_num;
    if (c == '\n') {
        screen_term_write("\n");
    } else if (c == '\b') {
        screen_term_write("\b");
    } else if (c >= 0x20 && c < 0x80) {
        char s[2] = {c, 0};
        screen_term_write(s);
    }
}

int tty_write(int tty_num, const char *buf, int count) {
    if (tty_num < 0 || tty_num >= TTY_NUM || !buf) return -1;
    for (int i = 0; i < count; i++) {
        tty_putchar(tty_num, buf[i]);
    }
    return count;
}

int tty_putchar(int tty_num, char c) {
    (void)tty_num;
    char s[2] = {c, 0};
    screen_term_write(s);
    return 1;
}

void tty_input(int tty_num, char c) {
    if (tty_num < 0 || tty_num >= TTY_NUM || !ttys[tty_num].active) return;

    tty_t *tty = &ttys[tty_num];

    if (c == '\b') {
        if (tty->input_head != tty->input_tail) {
            tty->input_head = (tty->input_head - 1 + TTY_BUF_SIZE) % TTY_BUF_SIZE;
            if (tty->echo) tty_echo(tty_num, c);
        }
        return;
    }

    int next = (tty->input_head + 1) % TTY_BUF_SIZE;
    if (next != tty->input_tail) {
        tty->input_buffer[tty->input_head] = c;
        tty->input_head = next;
        if (tty->echo) tty_echo(tty_num, c);
    }
}

int tty_set_foreground(int tty_num, int pid) {
    if (tty_num < 0 || tty_num >= TTY_NUM) return -1;
    ttys[tty_num].pid_foreground = pid;
    return 0;
}

int tty_get_foreground(int tty_num) {
    if (tty_num < 0 || tty_num >= TTY_NUM) return -1;
    return ttys[tty_num].pid_foreground;
}

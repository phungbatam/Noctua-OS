#ifndef SCREEN_H
#define SCREEN_H

#include <stddef.h>

#define SCROLL_BUF_LINES 500
#define HEADER_ROWS 6

enum vga_color {
    BLACK = 0, BLUE = 1, GREEN = 2, CYAN = 3,
    RED = 4, MAGENTA = 5, BROWN = 6, LIGHT_GREY = 7,
    DARK_GREY = 8, LIGHT_BLUE = 9, LIGHT_GREEN = 10,
    LIGHT_CYAN = 11, LIGHT_RED = 12, LIGHT_MAGENTA = 13,
    LIGHT_BROWN = 14, WHITE = 15,
};

#define C_TITLE      (LIGHT_BLUE << 4 | WHITE)
#define C_WIN_TITLE  (DARK_GREY << 4 | WHITE)
#define C_WIN_TEXT   (BLACK << 4 | LIGHT_GREEN)
#define C_PROMPT     (BLACK << 4 | LIGHT_CYAN)
#define C_INPUT      (BLACK << 4 | WHITE)
#define C_ERROR      (BLACK << 4 | LIGHT_RED)
#define C_INFO       (BLACK << 4 | CYAN)
#define C_HEADER     (BLACK << 4 | LIGHT_MAGENTA)
#define C_MOUSE      (WHITE << 4 | BLACK)

void screen_init(unsigned int mb_info);
void screen_clear_content(void);
void screen_term_putchar(char c);
void screen_term_write(const char *s);
void screen_set_content_color(unsigned char c);
void screen_draw_header(void);

int screen_get_term_x(void);
int screen_get_term_y(void);
void screen_set_term_pos(int x, int y);
void screen_term_write_buf(const char *s, int len);
void screen_scroll_up(void);
void screen_scroll_down(void);
int screen_scroll_offset(void);
void screen_scroll_reset(void);
void screen_redraw_content(void);

void draw_char_at(int x, int y, unsigned char c, unsigned char attr);

#define CONTENT_H   (fb_chars_y - HEADER_ROWS)
#define CONTENT_W   fb_chars_x

#endif

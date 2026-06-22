#include "screen.h"
#include "fb.h"
#include "string.h"
#include "page.h"
#include "rtc.h"
#include "cpuid.h"

extern const unsigned char font8x16[][16];

static unsigned char content_color = 0x0F;
static int term_row = 0;
static int term_col = 0;
static char header_cpu[48] = "Unknown";
static char current_line[80];

#define CONTENT_X   0
#define CONTENT_Y   HEADER_ROWS

static uint32_t hex_to_rgb(unsigned char c) {
    static const uint32_t pal[16] = {
        0x0A0A0A, 0xCC0000, 0x00CC00, 0xCCCC00,
        0x0000CC, 0xCC00CC, 0x00CCCC, 0xCCCCCC,
        0x444444, 0xFF4444, 0x44FF44, 0xFFFF44,
        0x4444FF, 0xFF44FF, 0x44FFFF, 0xFFFFFF,
    };
    return pal[c & 0x0F];
}
static uint32_t vga_fg(unsigned char attr) { return hex_to_rgb(attr & 0x0F); }
static uint32_t vga_bg(unsigned char attr) { return hex_to_rgb(attr >> 4); }

void draw_char_at(int x, int y, unsigned char c, unsigned char attr) {
    if (x < 0 || x >= fb_chars_x || y < 0 || y >= fb_chars_y) return;
    uint32_t fg = (attr & 0x0F) == 0 ? 0xCCCCCC : vga_fg(attr);
    uint32_t bg = vga_bg(attr);
    fb_char(x, y, c, fg, bg);
}

static void fill_line(unsigned char color, int y, int x1, int x2) {
    for (int x = x1; x <= x2; x++)
        draw_char_at(x, y, ' ', color);
}

static char scroll_buf[SCROLL_BUF_LINES][81];
static int scroll_count = 0;
static int scroll_pos = 0;
static int view_offset = 0;
static int in_scroll = 0;

static void buf_add_line(const char *s, int len) {
    int ml = len < 80 ? len : 80;
    for (int i = 0; i < ml; i++)
        scroll_buf[scroll_pos][i] = s[i] ? s[i] : ' ';
    scroll_buf[scroll_pos][ml] = 0;
    scroll_pos = (scroll_pos + 1) % SCROLL_BUF_LINES;
    if (scroll_count < SCROLL_BUF_LINES) scroll_count++;
}

static int buf_get_line(int idx, char *out) {
    if (idx < 0 || idx >= scroll_count) return 0;
    int actual = (scroll_pos - scroll_count + idx + SCROLL_BUF_LINES) % SCROLL_BUF_LINES;
    memcpy(out, scroll_buf[actual], 80);
    out[80] = 0;
    return 1;
}

static void scroll_content(void) {
    int cw = CONTENT_W < 80 ? CONTENT_W : 80;
    int start = scroll_count > CONTENT_H ? scroll_count - CONTENT_H : 0;
    for (int y = 0; y < CONTENT_H; y++) {
        char line[81];
        if (buf_get_line(start + y, line)) {
            for (int x = 0; x < cw; x++)
                draw_char_at(CONTENT_X + x, CONTENT_Y + y, line[x], content_color);
        } else {
            fill_line(content_color, CONTENT_Y + y, CONTENT_X, CONTENT_X + cw - 1);
        }
    }
}

static void flush_line_to_buf(void) {
    int cw = CONTENT_W < 80 ? CONTENT_W : 80;
    char line[81];
    for (int i = 0; i < cw; i++)
        line[i] = current_line[i] ? current_line[i] : ' ';
    line[cw] = 0;
    buf_add_line(line, cw);
    for (int i = 0; i < cw; i++)
        current_line[i] = 0;
}

static void draw_scrollbar(void) {
    int r = CONTENT_X + CONTENT_W - 1;
    uint32_t sbg = 0x1A1A1A;
    for (int y = CONTENT_Y; y < CONTENT_Y + CONTENT_H; y++) {
        int px = r * CHAR_W, py2 = y * CHAR_H;
        fb_fill(px, py2, 2, CHAR_H, sbg);
    }
    int max_offset = scroll_count - CONTENT_H;
    if (max_offset > 0 && scroll_count > 0) {
        if (view_offset > max_offset) view_offset = max_offset;
        int thumb = (view_offset * (CONTENT_H - 6)) / max_offset;
        int tpy = (CONTENT_Y + thumb) * CHAR_H;
        fb_fill(r * CHAR_W, tpy, 2, CHAR_H * 2, 0xCC0000);
    }
}

void screen_draw_header(void) {
    uint32_t mem_mb = pmem_total_pages() / 256;
    uint32_t free_mb = pmem_free_pages() / 256;
    uint32_t secs = uptime_get_seconds();
    uint32_t h = secs / 3600;
    uint32_t m = (secs % 3600) / 60;
    uint32_t s = secs % 60;

    int bx = 1, by = 0;
    int bw = fb_chars_x - 2, bh = 5;
    int px = bx * CHAR_W, py = by * CHAR_H;
    int pw = bw * CHAR_W, ph = bh * CHAR_H;

    fb_fill(px, py, pw, ph, 0x0D0D1A);
    fb_rect(px, py, pw, ph, 0x4444CC);

    uint32_t title_color = 0x4488FF;
    uint32_t text_color = 0x88AADD;

    fb_str(bx + 1, by + 0, "Noctua OS Kernel Console", title_color, 0x0D0D1A);

    char buf[80];
    fb_str(bx + 1, by + 1, "Kernel : Noctua Kernel", text_color, 0x0D0D1A);

    int2str((int)mem_mb, buf);
    char mem_str[40] = "Memory : ";
    strcat(mem_str, buf);
    strcat(mem_str, " MB");
    strcat(mem_str, " (");
    int2str((int)free_mb, buf);
    strcat(mem_str, buf);
    strcat(mem_str, " MB free)");
    fb_str(bx + 1, by + 2, mem_str, text_color, 0x0D0D1A);

    char time_str[16];
    int2str((int)h, buf);
    time_str[0] = 0;
    if (h < 10) strcat(time_str, "0");
    strcat(time_str, buf);
    strcat(time_str, ":");
    int2str((int)m, buf);
    if (m < 10) strcat(time_str, "0");
    strcat(time_str, buf);
    strcat(time_str, ":");
    int2str((int)s, buf);
    if (s < 10) strcat(time_str, "0");
    strcat(time_str, buf);

    char uptime_line[60] = "Uptime : ";
    strcat(uptime_line, time_str);
    while (strlen(uptime_line) < 30) strcat(uptime_line, " ");
    strcat(uptime_line, "CPU : ");
    strcat(uptime_line, header_cpu);
    fb_str(bx + 1, by + 3, uptime_line, text_color, 0x0D0D1A);
}

void screen_init(unsigned int mb_info) {
    get_cpu_vendor(header_cpu);
    if (header_cpu[0] == 0) memcpy(header_cpu, "Unknown", 8);

    fb_init(mb_info);

    fb_fill(0, 0, fb_width, fb_height, 0x0A0A0A);

    screen_draw_header();

    term_row = CONTENT_Y;
    term_col = CONTENT_X;
    for (int i = 0; i < 80; i++) current_line[i] = 0;
    view_offset = 0;
    in_scroll = 0;
    scroll_count = 0;
    scroll_pos = 0;
    screen_clear_content();
}

void screen_set_content_color(unsigned char c) { content_color = c; }

void screen_clear_content(void) {
    int cw = CONTENT_W < 80 ? CONTENT_W : 80;
    for (int y = CONTENT_Y; y < CONTENT_Y + CONTENT_H; y++)
        fill_line(content_color, y, CONTENT_X, CONTENT_X + cw - 1);
    term_row = CONTENT_Y;
    term_col = CONTENT_X;
    for (int i = 0; i < 80; i++) current_line[i] = 0;
    view_offset = 0;
    in_scroll = 0;
    scroll_count = 0;
    scroll_pos = 0;
}

void screen_redraw_content(void) {
    int cw = CONTENT_W < 80 ? CONTENT_W : 80;
    int start = scroll_count > CONTENT_H ? scroll_count - CONTENT_H - view_offset : 0;
    if (start < 0) start = 0;
    for (int y = 0; y < CONTENT_H; y++) {
        char line[81];
        if (buf_get_line(start + y, line)) {
            for (int x = 0; x < cw; x++)
                draw_char_at(CONTENT_X + x, CONTENT_Y + y, line[x], content_color);
        } else {
            fill_line(content_color, CONTENT_Y + y, CONTENT_X, CONTENT_X + cw - 1);
        }
    }
    draw_scrollbar();
}

void screen_scroll_up(void) {
    if (scroll_count <= CONTENT_H) return;
    if (view_offset + CONTENT_H < scroll_count) {
        view_offset++;
        in_scroll = 1;
        screen_redraw_content();
    }
}

void screen_scroll_down(void) {
    if (view_offset > 0) {
        view_offset--;
        if (view_offset == 0) in_scroll = 0;
        screen_redraw_content();
    }
}

int screen_scroll_offset(void) { return view_offset; }
void screen_scroll_reset(void) { view_offset = 0; in_scroll = 0; screen_redraw_content(); }
int screen_get_term_x(void) { return term_col; }
int screen_get_term_y(void) { return term_row; }
void screen_set_term_pos(int x, int y) { term_col = x; term_row = y; }

void screen_term_putchar(char c) {
    if (c == '\n') {
        flush_line_to_buf();
        term_col = CONTENT_X;
        term_row++;
    } else if (c == '\t') {
        int old = term_col - CONTENT_X;
        term_col = ((term_col - CONTENT_X + 4) & ~3) + CONTENT_X;
        for (int i = old; i < term_col - CONTENT_X; i++)
            current_line[i] = ' ';
    } else if (c == '\b') {
        if (term_col > CONTENT_X) {
            term_col--;
            current_line[term_col - CONTENT_X] = 0;
            draw_char_at(term_col, term_row, ' ', content_color);
        }
    } else {
        current_line[term_col - CONTENT_X] = c;
        draw_char_at(term_col, term_row, c, content_color);
        term_col++;
    }

    int cw = CONTENT_W < 80 ? CONTENT_W : 80;
    if (term_col >= CONTENT_X + cw) {
        flush_line_to_buf();
        term_col = CONTENT_X;
        term_row++;
    }

    if (term_row >= CONTENT_Y + CONTENT_H) {
        if (!in_scroll) {
            flush_line_to_buf();
            scroll_content();
            term_row = CONTENT_Y + CONTENT_H - 1;
        } else {
            term_row = CONTENT_Y + CONTENT_H - 1;
        }
    }
}

void screen_term_write(const char *s) { while (*s) screen_term_putchar(*s++); }
void screen_term_write_buf(const char *s, int len) { for (int i = 0; i < len; i++) screen_term_putchar(s[i]); }

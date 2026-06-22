#include "fb.h"
#include "mm/page.h"

volatile uint32_t *fb_ptr = 0;
int fb_width = FB_DEFAULT_W, fb_height = FB_DEFAULT_H;
int fb_pitch = FB_DEFAULT_W * 4;
int fb_bpp = 32;
int fb_chars_x = FB_DEFAULT_W / CHAR_W;
int fb_chars_y = FB_DEFAULT_H / CHAR_H;

static volatile uint32_t *fb_back_buffer = 0;
static int double_buffering = 0;

void fb_init(unsigned int mb_info) {
    unsigned int *mb = (unsigned int *)mb_info;
    unsigned int flags = mb[0];

    if (flags & (1 << 11)) {
        unsigned long long addr = *(unsigned long long *)(mb_info + 88);
        fb_ptr = (volatile uint32_t *)(unsigned long)addr;
        fb_pitch   = *(unsigned int *)(mb_info + 96);
        fb_width   = *(unsigned int *)(mb_info + 100);
        fb_height  = *(unsigned int *)(mb_info + 104);
        fb_bpp     = *(unsigned char *)(mb_info + 108);
    }
    fb_chars_x = fb_width / CHAR_W;
    fb_chars_y = fb_height / CHAR_H;
    double_buffering = 0;
    fb_back_buffer = 0;
}

void fb_double_buffer_init(void) {
    if (double_buffering && fb_back_buffer) return;
    int pitch = fb_pitch / 4;
    int size = pitch * fb_height;
    int pages = (size * 4 + 4095) / 4096;
    uint32_t *buf = 0;
    for (int i = 0; i < pages; i++) {
        uint32_t *page = (uint32_t *)pmem_alloc_page();
        if (!page) return;
        if (i == 0) buf = page;
    }
    fb_back_buffer = (volatile uint32_t *)buf;
    if (fb_back_buffer) {
        double_buffering = 1;
    }
}

void fb_swap(void) {
    if (!double_buffering || !fb_back_buffer || !fb_ptr) return;
    int pitch = fb_pitch / 4;
    int size = pitch * fb_height;
    for (int i = 0; i < size; i++) {
        fb_ptr[i] = fb_back_buffer[i];
    }
}

void fb_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= fb_width || y < 0 || y >= fb_height || !fb_ptr) return;
    fb_ptr[y * (fb_pitch / 4) + x] = color;
}

void fb_fill(int x, int y, int w, int h, uint32_t color) {
    for (int row = 0; row < h && y + row < fb_height; row++)
        for (int col = 0; col < w && x + col < fb_width; col++)
            fb_pixel(x + col, y + row, color);
}

void fb_rect(int x, int y, int w, int h, uint32_t color) {
    int x2 = x + w - 1, y2 = y + h - 1;
    for (int i = x; i <= x2 && i < fb_width; i++) {
        if (y >= 0 && y < fb_height) fb_pixel(i, y, color);
        if (y2 >= 0 && y2 < fb_height && y2 != y) fb_pixel(i, y2, color);
    }
    for (int i = y; i <= y2 && i < fb_height; i++) {
        if (x >= 0 && x < fb_width) fb_pixel(x, i, color);
        if (x2 >= 0 && x2 < fb_width && x2 != x) fb_pixel(x2, i, color);
    }
}

uint32_t fb_rgb(unsigned char r, unsigned char g, unsigned char b) {
    return (r << 16) | (g << 8) | b;
}

extern const unsigned char font8x16[][16];

static void draw_glyph(int px, int py, unsigned char c, uint32_t fg, uint32_t bg) {
    for (int row = 0; row < CHAR_H; row++) {
        unsigned char bits = font8x16[c][row];
        for (int col = 0; col < CHAR_W; col++) {
            fb_pixel(px + col, py + row, (bits >> (7 - col)) & 1 ? fg : bg);
        }
    }
}

void fb_char(int x, int y, unsigned char c, uint32_t fg, uint32_t bg) {
    draw_glyph(x * CHAR_W, y * CHAR_H, c, fg, bg);
}

void fb_str(int x, int y, const char *s, uint32_t fg, uint32_t bg) {
    while (*s) {
        fb_char(x, y, (unsigned char)*s, fg, bg);
        x++;
        if (x >= fb_chars_x) { x = 0; y++; }
        s++;
    }
}

void fb_char_ex(int x, int y, unsigned char c, uint32_t fg, uint32_t bg, int scale) {
    if (scale <= 1) { fb_char(x, y, c, fg, bg); return; }
    int px = x * CHAR_W, py = y * CHAR_H;
    for (int row = 0; row < CHAR_H; row++) {
        unsigned char bits = font8x16[c][row];
        for (int col = 0; col < CHAR_W; col++) {
            uint32_t colr = (bits >> (7 - col)) & 1 ? fg : bg;
            for (int sy = 0; sy < scale; sy++)
                for (int sx = 0; sx < scale; sx++)
                    fb_pixel(px + col * scale + sx, py + row * scale + sy, colr);
        }
    }
}
#ifndef FB_H
#define FB_H

#include <stdint.h>

#define CHAR_W 8
#define CHAR_H 16
#define FB_DEFAULT_W 640
#define FB_DEFAULT_H 400

extern volatile uint32_t *fb_ptr;
extern int fb_width, fb_height, fb_pitch, fb_bpp;
extern int fb_chars_x, fb_chars_y;

void fb_double_buffer_init(void);
void fb_swap(void);
void fb_init(unsigned int mb_info);
void fb_pixel(int x, int y, uint32_t color);
void fb_fill(int x, int y, int w, int h, uint32_t color);
void fb_char(int x, int y, unsigned char c, uint32_t fg, uint32_t bg);
void fb_str(int x, int y, const char *s, uint32_t fg, uint32_t bg);
void fb_char_ex(int x, int y, unsigned char c, uint32_t fg, uint32_t bg, int scale);
void fb_rect(int x, int y, int w, int h, uint32_t color);

uint32_t fb_rgb(unsigned char r, unsigned char g, unsigned char b);

#endif
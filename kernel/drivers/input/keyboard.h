#ifndef KEYBOARD_H
#define KEYBOARD_H

#define K_F1      0x80
#define K_F2      0x81
#define K_PGUP    0x82
#define K_PGDN    0x83
#define K_UP      0x84
#define K_DOWN    0x85
#define K_LEFT    0x86
#define K_RIGHT   0x87
#define K_HOME    0x88
#define K_END     0x89
#define K_DEL     0x8A
#define K_INS     0x8B
#define K_MOUSE_L 0x8C
#define K_ESC     0x1B
#define K_ENTER   0x0A
#define K_TAB     0x09
#define K_BKSP    0x08
#define K_CTRL_C  0x8D

void keyboard_init(void);
int keyboard_getchar(void);
int keyboard_getchar_nb(void);  // non-blocking version
int keyboard_ctrl(void);

#endif

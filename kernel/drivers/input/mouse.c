#include "mouse.h"
#include "fb.h"
#include "ports.h"

static int mouse_x, mouse_y;
static int mouse_buttons = 0;
static int mouse_pkt = 0;
static unsigned char mouse_buf[3];
static int mouse_visible = 0;
static int click_pending = 0;
static int prev_btn = 0;

static void wait_write(void) {
    for (volatile int i = 0; i < 1000; i++)
        if (!(inb(0x64) & 0x02)) return;
}

static void mouse_write(unsigned char cmd) {
    wait_write();
    outb(0x64, 0xD4);
    wait_write();
    outb(0x60, cmd);
    for (volatile int i = 0; i < 10000; i++)
        if (inb(0x64) & 0x01) { inb(0x60); return; }
}

void mouse_init(void) {
    wait_write(); outb(0x64, 0xA8);
    wait_write(); outb(0x64, 0x20);
    for (volatile int i = 0; i < 10000; i++)
        if (inb(0x64) & 0x01) break;
    unsigned char cfg = inb(0x60) | 0x02;
    wait_write(); outb(0x64, 0x60);
    wait_write(); outb(0x60, cfg);

    mouse_write(0xF6);
    mouse_write(0xF4);

    while (inb(0x64) & 0x01) inb(0x60);
    mouse_visible = 1;
    mouse_x = fb_width / 2;
    mouse_y = fb_height / 2;
    prev_btn = 0;
    click_pending = 0;
    mouse_pkt = 0;
}

void mouse_handle_byte(unsigned char byte)
{
    if (!mouse_visible)
        return;

    if (mouse_pkt == 0 && !(byte & 0x08))
        return;

    mouse_buf[mouse_pkt++] = byte;

    if (mouse_pkt < 3)
        return;

    mouse_pkt = 0;

    int dx = (signed char)mouse_buf[1];
    int dy = -(signed char)mouse_buf[2];

    mouse_x += dx;
    mouse_y += dy;

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= fb_width) mouse_x = fb_width - 1;

    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= fb_height) mouse_y = fb_height - 1;

    mouse_buttons = mouse_buf[0] & 0x07;

    if ((mouse_buttons & 0x01) && !(prev_btn & 0x01))
        click_pending = 1;

    prev_btn = mouse_buttons;
}

int mouse_has_click(void) {
    if (click_pending) { click_pending = 0; return 1; }
    return 0;
}

int mouse_get_x(void) { return mouse_x; }
int mouse_get_y(void) { return mouse_y; }
int mouse_get_buttons(void) { return mouse_buttons; }

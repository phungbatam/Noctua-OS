#include "printk.h"
#include "vsprintf.h"
#include "screen.h"
#include "string.h"
#include "serial.h"
#include "pit.h"

static uint32_t boot_start_ticks = 0;
static int pit_ready = 0;
static uint32_t early_counter = 0;

void printk_init(void) {
    boot_start_ticks = 0;
    early_counter = 0;
    pit_ready = 0;
}

static void print_number(uint32_t val, int width, char pad) {
    char buf[16];
    int i = 0;
    if (val == 0) {
        buf[i++] = '0';
    } else {
        while (val > 0 && i < 14) {
            buf[i++] = '0' + (val % 10);
            val /= 10;
        }
    }
    while (i < width) buf[i++] = pad;
    for (int j = i - 1; j >= 0; j--) {
        screen_term_putchar(buf[j]);
        serial_write_char(COM1_PORT, buf[j]);
    }
}

void printk_timestamp(void) {
    unsigned long seconds;
    unsigned long micros;

    if (pit_ready) {
        uint32_t ticks;
        if (boot_start_ticks == 0) {
            boot_start_ticks = pit_get_ticks();
            ticks = 0;
        } else {
            ticks = pit_get_ticks() - boot_start_ticks;
        }
        seconds = ticks / TICK_HZ;
        micros = (ticks % TICK_HZ) * (1000000 / TICK_HZ);
    } else {
        seconds = early_counter / 100;
        micros = (early_counter % 100) * 10000;
    }

    screen_term_putchar('[');
    serial_write_char(COM1_PORT, '[');
    print_number(seconds, 5, ' ');
    screen_term_putchar('.');
    serial_write_char(COM1_PORT, '.');
    print_number(micros, 6, '0');
    screen_term_putchar(']');
    screen_term_putchar(' ');
    serial_write_char(COM1_PORT, ']');
    serial_write_char(COM1_PORT, ' ');
}

void printk(const char *fmt, ...) {
    if (!fmt) return;

    if (!pit_ready) {
        early_counter++;
    }

    printk_timestamp();

    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    const char *p = buf;
    while (*p) {
        screen_term_putchar(*p);
        serial_write_char(COM1_PORT, *p);
        p++;
    }

    screen_term_putchar('\n');
    serial_write_char(COM1_PORT, '\n');
}

void printk_set_pit_ready(void) {
    pit_ready = 1;
    boot_start_ticks = pit_get_ticks();
}

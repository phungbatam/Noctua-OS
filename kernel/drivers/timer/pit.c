#include "pit.h"
#include "ports.h"
#include <stddef.h>

static volatile uint32_t timer_ticks = 0;
void (*pit_on_tick)(void) = NULL;

void pit_init(uint32_t frequency) {
    uint32_t divisor = PIT_FREQUENCY / frequency;

    /* Gửi command: channel 0, lobyte/hibyte, mode 2 (rate generator) */
    outb(PIT_PORT_CMD, 0x36);
    outb(PIT_PORT_CH0, divisor & 0xFF);
    outb(PIT_PORT_CH0, (divisor >> 8) & 0xFF);

    timer_ticks = 0;
}

/* Gọi từ IRQ0 handler */
void pit_tick(void) {
    timer_ticks++;
    if (pit_on_tick)
        pit_on_tick();
}

uint32_t pit_get_ticks(void) {
    return timer_ticks;
}

void pit_sleep(uint32_t ms) {
    uint32_t start = timer_ticks;
    uint32_t delay = (ms * TICK_HZ) / 1000;
    if (delay == 0) delay = 1;
    while (timer_ticks - start < delay) {
        asm volatile("hlt");
    }
}
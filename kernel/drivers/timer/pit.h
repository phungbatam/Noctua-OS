#ifndef TVN_PIT_H
#define TVN_PIT_H

#include <stdint.h>

/* PIT (Programmable Interval Timer) - IRQ0 */
/* Dùng để tạo timer tick cho scheduler */

#define PIT_PORT_CH0    0x40
#define PIT_PORT_CMD    0x43
#define PIT_FREQUENCY   1193182
#define TICK_HZ         100 /* 100 ticks/second */

void pit_init(uint32_t frequency);
void pit_tick(void);
uint32_t pit_get_ticks(void);
void pit_sleep(uint32_t ms);

extern void (*pit_on_tick)(void);

#endif
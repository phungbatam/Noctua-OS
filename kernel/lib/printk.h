#ifndef NOCTUA_PRINTK_H
#define NOCTUA_PRINTK_H

#include <stdint.h>
#include <stdarg.h>

void printk_init(void);
void printk(const char *fmt, ...);
void printk_timestamp(void);
void printk_set_pit_ready(void);

#endif

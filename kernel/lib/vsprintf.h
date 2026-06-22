#ifndef NOCTUA_VSPRINTF_H
#define NOCTUA_VSPRINTF_H

#include <stdint.h>
#include <stdarg.h>

int vsnprintf(char *buf, uint32_t size, const char *fmt, va_list args);
int snprintf(char *buf, uint32_t size, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);

#endif

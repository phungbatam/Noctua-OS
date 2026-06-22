#ifndef NOCTUA_SERIAL_H
#define NOCTUA_SERIAL_H

#include <stdint.h>

#define COM1_PORT 0x3F8
#define COM2_PORT 0x2F8
#define COM3_PORT 0x3E8
#define COM4_PORT 0x2E8

#define SERIAL_DATA_REG       0
#define SERIAL_INTR_ENABLE    1
#define SERIAL_FIFO_CTRL      2
#define SERIAL_LINE_CTRL      3
#define SERIAL_MODEM_CTRL     4
#define SERIAL_LINE_STATUS    5
#define SERIAL_MODEM_STATUS   6
#define SERIAL_SCRATCH        7

#define SERIAL_LCR_DLAB       0x80
#define SERIAL_LCR_8N1        0x03
#define SERIAL_LCR_7E1        0x0A

#define SERIAL_LSR_DATA_READY 0x01
#define SERIAL_LSR_THR_EMPTY  0x20
#define SERIAL_LSR_TX_EMPTY   0x40

#define SERIAL_MCR_DTR        0x01
#define SERIAL_MCR_RTS        0x02
#define SERIAL_MCR_OUT2       0x08

void serial_init(void);
void serial_init_port(uint16_t port);
int serial_received(uint16_t port);
char serial_read_char(uint16_t port);
void serial_write_char(uint16_t port, char c);
void serial_write_str(uint16_t port, const char *str);
void serial_write_hex(uint16_t port, uint32_t value);
void serial_write_dec(uint16_t port, uint32_t value);
void serial_printf(const char *fmt, ...);

void debug_printf(const char *fmt, ...);
void debug_putchar(char c);
void debug_write(const char *str);

#endif

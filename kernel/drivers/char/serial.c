#include "serial.h"
#include "ports.h"
#include "string.h"

static int serial_initialized = 0;
static uint16_t debug_port = COM1_PORT;

void serial_init_port(uint16_t port) {
    outb(port + SERIAL_INTR_ENABLE, 0x00);
    outb(port + SERIAL_LINE_CTRL, SERIAL_LCR_DLAB);
    outb(port + SERIAL_DATA_REG, 0x01);
    outb(port + SERIAL_INTR_ENABLE, 0x00);
    outb(port + SERIAL_LINE_CTRL, SERIAL_LCR_8N1);
    outb(port + SERIAL_FIFO_CTRL, 0xC7);
    outb(port + SERIAL_MODEM_CTRL, SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_OUT2);
    outb(port + SERIAL_INTR_ENABLE, 0x00);
}

void serial_init(void) {
    serial_init_port(COM1_PORT);
    serial_init_port(COM2_PORT);
    serial_initialized = 1;
    serial_write_str(COM1_PORT, "\r\n[Noctua OS] Serial console initialized.\r\n");
}

int serial_received(uint16_t port) {
    return inb(port + SERIAL_LINE_STATUS) & SERIAL_LSR_DATA_READY;
}

char serial_read_char(uint16_t port) {
    while (!serial_received(port));
    return inb(port + SERIAL_DATA_REG);
}

void serial_write_char(uint16_t port, char c) {
    while (!(inb(port + SERIAL_LINE_STATUS) & SERIAL_LSR_THR_EMPTY));
    outb(port + SERIAL_DATA_REG, c);

    if (c == '\n') {
        while (!(inb(port + SERIAL_LINE_STATUS) & SERIAL_LSR_THR_EMPTY));
        outb(port + SERIAL_DATA_REG, '\r');
    }
}

void serial_write_str(uint16_t port, const char *str) {
    if (!str) return;
    while (*str) {
        serial_write_char(port, *str++);
    }
}

void serial_write_hex(uint16_t port, uint32_t value) {
    const char *hex = "0123456789ABCDEF";
    serial_write_char(port, '0');
    serial_write_char(port, 'x');
    for (int i = 28; i >= 0; i -= 4) {
        serial_write_char(port, hex[(value >> i) & 0x0F]);
    }
}

void serial_write_dec(uint16_t port, uint32_t value) {
    char buf[12];
    int i = 0;
    if (value == 0) {
        serial_write_char(port, '0');
        return;
    }
    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }
    while (i > 0) {
        serial_write_char(port, buf[--i]);
    }
}

void serial_printf(const char *fmt, ...) {
    (void)fmt;
}

void debug_putchar(char c) {
    serial_write_char(debug_port, c);
}

void debug_write(const char *str) {
    serial_write_str(debug_port, str);
}

void debug_printf(const char *fmt, ...) {
    serial_write_str(debug_port, "[DEBUG] ");
    serial_write_str(debug_port, fmt ? fmt : "null");
    serial_write_str(debug_port, "\r\n");
}

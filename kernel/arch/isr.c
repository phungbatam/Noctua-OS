#include "isr.h"
#include "screen.h"
#include "ports.h"
#include "string.h"
#include "proc/sched.h"
#include "timer/pit.h"
#include "input/keyboard.h"
#include "input/mouse.h"
#include "mm/page.h"

static const char *exception_messages[] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Into Detected Overflow", "Out of Bounds", "Invalid Opcode", "No Coprocessor",
    "Double Fault", "Coprocessor Segment Overrun", "Bad TSS", "Segment Not Present",
    "Stack Fault", "General Protection Fault", "Page Fault", "Unknown Interrupt",
    "Coprocessor Fault", "Alignment Check", "Machine Check", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved"
};

static void (*irq_handlers[16])(struct registers *r);

void irq_register_handler(int irq, void (*handler)(struct registers *r)) {
    if (irq >= 0 && irq < 16)
        irq_handlers[irq] = handler;
}

void isr_handler(struct registers *r) {
    if (r->int_no < 32) {
        screen_set_content_color(C_ERROR);
        screen_term_write("EXCEPTION: ");
        screen_term_write(exception_messages[r->int_no]);
        screen_term_write(" (");
        char hex[] = "0x00";
        const char *hex_chars = "0123456789ABCDEF";
        hex[2] = hex_chars[(r->int_no >> 4) & 0xF];
        hex[3] = hex_chars[r->int_no & 0xF];
        screen_term_write(hex);
        screen_term_write(")\nSYSTEM HALTED\n");
        for (;;);
    }
}

void irq_handler(struct registers *r) {
    int irq = r->int_no - 32;

    /* Gọi handler đã đăng ký */
    if (irq >= 0 && irq < 16 && irq_handlers[irq])
        irq_handlers[irq](r);

    /* Timer IRQ0 - gọi scheduler */
    if (irq == 0) {
        pit_tick();
        sched_switch(r);
    }

    /* Keyboard IRQ1 - xử lý keyboard */
    if (irq == 1) {
        /* Keyboard đã được poll trong main loop,
         * nhưng handler để đánh thức task đang chờ */
    }

    /* Gửi EOI */
    if (irq >= 8)
        outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

void irq_install(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x00);
    outb(0xA1, 0x00);
}

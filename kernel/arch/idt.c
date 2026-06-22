#include "idt.h"
#include "string.h"

static struct idt_entry idt[256];
static struct idt_ptr idtp;

extern unsigned long isr_table[];
extern unsigned long irq_table[];
extern void idt_load(struct idt_ptr *ptr);

void idt_set_gate(unsigned char num, unsigned long base, unsigned short sel, unsigned char flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt;
    memset(&idt, 0, sizeof(idt));

    for (int i = 0; i < 32; i++)
        idt_set_gate(i, isr_table[i], 0x08, 0x8E);

    for (int i = 0; i < 16; i++)
        idt_set_gate(32 + i, irq_table[i], 0x08, 0x8E);

    idt_load(&idtp);
}

extern void syscall_int(void);

void syscall_init_idt(void) {
    idt_set_gate(0x80, (unsigned long)syscall_int, 0x08, 0xEE);
}

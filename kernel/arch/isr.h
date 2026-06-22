#ifndef ISR_H
#define ISR_H

struct registers {
    unsigned int gs, fs, es, ds;
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
    unsigned int int_no, err_code;
    unsigned int eip, cs, eflags, useresp, ss;
};

void isr_handler(struct registers *r);
void irq_handler(struct registers *r);
void irq_install(void);
void irq_register_handler(int irq, void (*handler)(struct registers *r));

#endif

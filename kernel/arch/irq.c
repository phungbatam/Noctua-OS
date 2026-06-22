#include "irq.h"
#include "string.h"

static irq_desc_t irq_desc[NR_IRQS];

void irq_init(void) {
    static const char *names[] = {
        "timer", "keyboard", "cascade", "serial2", "serial1", "parallel2",
        "floppy", "parallel1", "rtc", "acpi", "input10", "input11",
        "psaux", "fpu", "ata-primary", "ata-secondary"
    };
    
    for (int i = 0; i < NR_IRQS; i++) {
        irq_desc[i].action = 0;
        irq_desc[i].irq_count = 0;
        irq_desc[i].flags = 0;
        irq_desc[i].name = names[i];
    }
}

int irq_register(int irq, irqaction_t *action) {
    if (irq < 0 || irq >= NR_IRQS || !action) return -1;

    if (!irq_desc[irq].action) {
        irq_desc[irq].action = action;
        action->next = 0;
    } else {
        if (!(irq_desc[irq].action->flags & IRQF_SHARED) &&
            !(action->flags & IRQF_SHARED)) {
            return -1;
        }
        action->next = irq_desc[irq].action;
        irq_desc[irq].action = action;
    }
    return 0;
}

void irq_unregister(int irq, irqaction_t *action) {
    if (irq < 0 || irq >= NR_IRQS || !action || !irq_desc[irq].action) return;

    irqaction_t **pp = &irq_desc[irq].action;
    while (*pp) {
        if (*pp == action) {
            *pp = action->next;
            action->next = 0;
            return;
        }
        pp = &(*pp)->next;
    }
}

irq_desc_t *irq_get_desc(int irq) {
    if (irq < 0 || irq >= NR_IRQS) return 0;
    return &irq_desc[irq];
}

void irq_handle_entry(int irq, void *regs) {
    if (irq < 0 || irq >= NR_IRQS) return;

    irq_desc_t *desc = &irq_desc[irq];
    desc->irq_count++;

    irqaction_t *a = desc->action;
    while (a) {
        if (a->handler) a->handler(regs);
        a = a->next;
    }
}

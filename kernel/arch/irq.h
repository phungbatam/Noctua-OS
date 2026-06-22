#ifndef TVN_IRQ_H
#define TVN_IRQ_H

#include <stdint.h>

#define NR_IRQS         16
#define IRQF_SHARED     0x0001
#define IRQF_TRIGGER_EDGE 0x0004

typedef struct irqaction {
    void (*handler)(void *regs);
    void *dev_id;
    uint32_t flags;
    const char *name;
    struct irqaction *next;
} irqaction_t;

typedef struct irq_desc {
    irqaction_t *action;
    uint32_t     irq_count;
    uint32_t     flags;
    const char   *name;
} irq_desc_t;

void irq_init(void);
int  irq_register(int irq, irqaction_t *action);
void irq_unregister(int irq, irqaction_t *action);
irq_desc_t *irq_get_desc(int irq);
void irq_handle_entry(int irq, void *regs);

#endif

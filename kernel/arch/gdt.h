#ifndef NOCTUA_GDT_H
#define NOCTUA_GDT_H

#include <stdint.h>

/* GDT access byte flags */
#define GDT_ACC_ACCESSED   0x01
#define GDT_ACC_RW         0x02
#define GDT_ACC_DIRECTION  0x04
#define GDT_ACC_EXEC       0x08
#define GDT_ACC_NONSYS     0x10
#define GDT_ACC_DPL(n)     ((n) << 5)
#define GDT_ACC_PRESENT    0x80

/* GDT flags (nibble 4) */
#define GDT_FLAG_64BIT     0x20
#define GDT_FLAG_32BIT     0x40
#define GDT_FLAG_GRAN_4K   0x80

/* Segment selectors */
#define GDT_KERNEL_CODE    0x08
#define GDT_KERNEL_DATA    0x10
#define GDT_USER_CODE      0x1B
#define GDT_USER_DATA      0x23
#define GDT_TSS_SEL        0x28

#ifndef ASM_FILE
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

typedef struct {
    uint16_t link;
    uint16_t reserved0;
    uint32_t esp0;
    uint16_t ss0;
    uint16_t reserved1;
    uint32_t esp1;
    uint16_t ss1;
    uint16_t reserved2;
    uint32_t esp2;
    uint16_t ss2;
    uint16_t reserved3;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed)) tss_entry_t;

void gdt_init(void);
void gdt_set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags);
void tss_set_stack(uint32_t esp0, uint32_t ss0);
uint32_t tss_get_stack(void);
void gdt_load_tss(void);

/* Assembly function to enter user mode */
void enter_user_mode(uint32_t eip, uint32_t esp, uint32_t stack_seg, uint32_t code_seg);
#endif

#endif

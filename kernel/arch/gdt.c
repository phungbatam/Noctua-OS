#include "gdt.h"
#include "string.h"
#include "screen.h"

static gdt_entry_t gdt_entries[7];
static gdt_ptr_t gdt_ptr;
static tss_entry_t tss_entry;

static void gdt_flush(uint32_t gdt_ptr) {
    __asm__ volatile (
        "lgdt (%0)\n\t"          // Load GDT descriptor from memory address gdt_ptr
        "ljmpl $0x08, $1f\n\t"   // Far jump to 0x08:next_inst (loads CS with code selector)
        "1:\n\t"                 // Local label 1 (jump target)
        "movw $0x10, %%ax\n\t"   // Load data segment selector (0x10) into AX
        "movw %%ax, %%ds\n\t"    // Reload DS
        "movw %%ax, %%es\n\t"    // Reload ES
        "movw %%ax, %%fs\n\t"    // Reload FS
        "movw %%ax, %%gs\n\t"    // Reload GS
        "movw %%ax, %%ss"        // Reload SS (stack segment)
        :                        // No output operands
        : "r"(gdt_ptr)           // Input: gdt_ptr passed via any general-purpose register
        : "eax", "memory"        // Clobbered: EAX and memory contents
    );
}

void gdt_set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    gdt_entries[i].base_low  = base & 0xFFFF;
    gdt_entries[i].base_mid  = (base >> 16) & 0xFF;
    gdt_entries[i].base_high = (base >> 24) & 0xFF;
    gdt_entries[i].limit_low = limit & 0xFFFF;
    gdt_entries[i].granularity = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    gdt_entries[i].access    = access;
}

void gdt_init(void) {
    gdt_ptr.limit = sizeof(gdt_entries) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    /* NULL descriptor */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* Kernel Code: base=0, limit=4GB, ring 0, code, 32-bit, 4K granularity */
    gdt_set_entry(1, 0, 0xFFFFF,
        GDT_ACC_PRESENT | GDT_ACC_NONSYS | GDT_ACC_EXEC | GDT_ACC_RW,
        GDT_FLAG_GRAN_4K | GDT_FLAG_32BIT);

    /* Kernel Data: base=0, limit=4GB, ring 0, data, writable, 32-bit, 4K granularity */
    gdt_set_entry(2, 0, 0xFFFFF,
        GDT_ACC_PRESENT | GDT_ACC_NONSYS | GDT_ACC_RW,
        GDT_FLAG_GRAN_4K | GDT_FLAG_32BIT);

    /* User Code: base=0, limit=4GB, ring 3, code, 32-bit, 4K granularity */
    gdt_set_entry(3, 0, 0xFFFFF,
        GDT_ACC_PRESENT | GDT_ACC_NONSYS | GDT_ACC_EXEC | GDT_ACC_RW | GDT_ACC_DPL(3),
        GDT_FLAG_GRAN_4K | GDT_FLAG_32BIT);

    /* User Data: base=0, limit=4GB, ring 3, data, writable, 32-bit, 4K granularity */
    gdt_set_entry(4, 0, 0xFFFFF,
        GDT_ACC_PRESENT | GDT_ACC_NONSYS | GDT_ACC_RW | GDT_ACC_DPL(3),
        GDT_FLAG_GRAN_4K | GDT_FLAG_32BIT);

    /* TSS descriptor */
    uint32_t tss_base = (uint32_t)&tss_entry;
    uint32_t tss_limit = sizeof(tss_entry_t) - 1;

    gdt_set_entry(5, tss_base, tss_limit,
        GDT_ACC_PRESENT | GDT_ACC_EXEC | GDT_ACC_ACCESSED | GDT_ACC_DPL(3),
        0x00);

    gdt_flush((uint32_t)&gdt_ptr);
    __asm__ volatile ("ltr %%ax" : : "a"(GDT_TSS_SEL));


}

void tss_set_stack(uint32_t esp0, uint32_t ss0) {
    tss_entry.esp0 = esp0;
    tss_entry.ss0  = ss0;
    tss_entry.iomap_base = sizeof(tss_entry_t);
}

uint32_t tss_get_stack(void) {
    return tss_entry.esp0;
}

void gdt_load_tss(void) {
    __asm__ volatile ("ltr %%ax" : : "a"(GDT_TSS_SEL));
}

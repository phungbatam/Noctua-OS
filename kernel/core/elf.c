#include "elf.h"
#include "screen.h"
#include "string.h"
#include "mm/page.h"
#include "mm/paging.h"
#include "mm/heap.h"
#include "proc/task.h"
#include "proc/sched.h"
#include "fs/fat32.h"

int elf_validate(elf_header_t *header) {
    if (!header) return 0;
    if (header->e_ident[EI_MAG0] != ELFMAG0) return 0;
    if (header->e_ident[EI_MAG1] != ELFMAG1) return 0;
    if (header->e_ident[EI_MAG2] != ELFMAG2) return 0;
    if (header->e_ident[EI_MAG3] != ELFMAG3) return 0;
    if (header->e_ident[EI_CLASS] != ELFCLASS32) return 0;
    if (header->e_ident[EI_DATA] != ELFDATA2LSB) return 0;
    if (header->e_machine != EM_386) return 0;
    if (header->e_type != ET_EXEC && header->e_type != ET_DYN) return 0;
    return 1;
}

static int elf_load_segment(elf_header_t *header, elf_program_header_t *ph,
                            struct vfs_node *node, uint32_t base_addr) {
    (void)header;

    /* Align start/end */
    uint32_t vaddr = base_addr + ph->p_vaddr;
    uint32_t memsz = ph->p_memsz;
    uint32_t filesz = ph->p_filesz;
    uint32_t offset = ph->p_offset;

    /* Round down to page boundary */
    uint32_t page_start = vaddr & ~0xFFF;
    uint32_t page_end = ((vaddr + memsz + 0xFFF) & ~0xFFF);
    uint32_t page_offset = vaddr - page_start;

    /* Allocate pages for this segment */
    for (uint32_t p = page_start; p < page_end; p += PAGE_SIZE) {
        void *phys = pmem_alloc_page();
        if (!phys) return -1;
        paging_map_page((void *)p, phys, PAGE_PRESENT | PAGE_WRITE);
    }

    /* Read file data into memory */
    if (filesz > 0) {
        uint8_t *temp = (uint8_t *)kmalloc(filesz);
        if (!temp) return -1;

        if (node->f_op && node->f_op->read) {
            node->f_op->read(node, offset, filesz, temp);
        }

        /* Copy to the mapped address */
        uint8_t *dest = (uint8_t *)(page_start + page_offset);
        memcpy(dest, temp, filesz);

        kfree(temp);
    }

    /* Zero-fill BSS (memsz > filesz) */
    if (memsz > filesz) {
        uint8_t *bss_start = (uint8_t *)(page_start + page_offset + filesz);
        memset(bss_start, 0, memsz - filesz);
    }

    return 0;
}

elf_load_result_t elf_load(struct vfs_node *node) {
    elf_load_result_t result = {0, 0, 0, 4096};
    char buf[512];

    if (!node) return result;

    /* Read ELF header (first 512 bytes) */
    if (!node->f_op || !node->f_op->read) return result;

    int bytes = node->f_op->read(node, 0, 512, buf);
    if (bytes < (int)sizeof(elf_header_t)) return result;

    elf_header_t *header = (elf_header_t *)buf;

    /* Validate */
    if (!elf_validate(header)) {
        screen_set_content_color(C_ERROR);
        screen_term_write("ELF: Invalid header\n");
        return result;
    }

    /* Determine load address */
    uint32_t base_addr = 0x08048000; /* Default ELF base */
    uint32_t max_end = 0;

    /* Load each segment */
    for (int i = 0; i < header->e_phnum; i++) {
        uint32_t ph_off = header->e_phoff + i * header->e_phentsize;

        /* Read program header */
        elf_program_header_t ph;
        node->f_op->read(node, ph_off, sizeof(ph), (char *)&ph);

        if (ph.p_type == PT_LOAD) {
            if (elf_load_segment(header, &ph, node, base_addr) < 0) {
                screen_set_content_color(C_ERROR);
                screen_term_write("ELF: Failed to load segment\n");
                return result;
            }
            uint32_t seg_end = base_addr + ph.p_vaddr + ph.p_memsz;
            if (seg_end > max_end) max_end = seg_end;
        }
    }

    result.success = 1;
    result.entry_point = base_addr + header->e_entry;
    result.load_addr = base_addr;

    return result;
}

/* Task entry point trampoline for ELF tasks */
static void elf_task_entry(void) {
    /* The actual entry point is stored in the task context
     * This function is never called directly - the task's context.eip
     * is set to the ELF entry point before scheduling */
    for (;;) {
        task_yield();
    }
}

int elf_load_task(struct vfs_node *node, const char *name) {
    elf_load_result_t result = elf_load(node);
    if (!result.success) return -1;

    /* Create a task for this ELF */
    int tid = task_create(name, elf_task_entry, 1);
    if (tid < 0) return -1;

    /* Set the task's entry point */
    task_t *t = task_find_by_pid(tid);
    if (t) {
        t->context.eip = result.entry_point;
        /* Set up user-mode stack */
        uint32_t *stack = (uint32_t *)pmem_alloc_page();
        if (stack) {
            uint32_t stack_top = (uint32_t)stack + PAGE_SIZE - 16;
            t->context.esp = stack_top;
            t->context.eflags = 0x202;
        }
    }

    screen_set_content_color(C_INFO);
    screen_term_write("ELF: Loaded '");
    screen_term_write(name);
    screen_term_write("' at 0x");
    char hex[16];
    int2str((int)result.entry_point, hex);
    screen_term_write(hex);
    screen_term_write("\n");

    return tid;
}

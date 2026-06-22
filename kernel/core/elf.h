#ifndef TVN_ELF_H
#define TVN_ELF_H

#include <stdint.h>

/* ELF magic number */
#define ELF_MAGIC 0x464C457F

/* ELF header (32-bit) */
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf_header_t;

/* ELF program header */
typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) elf_program_header_t;

/* ELF section header */
typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} __attribute__((packed)) elf_section_header_t;

/* Program header types */
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_PHDR    6

/* Section header types */
#define SHT_NULL     0
#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHT_RELA     4
#define SHT_HASH     5
#define SHT_DYNAMIC  6
#define SHT_NOTE     7
#define SHT_NOBITS   8
#define SHT_REL      9
#define SHT_DYNSYM   11

/* ELF identification indices */
#define EI_MAG0    0
#define EI_MAG1    1
#define EI_MAG2    2
#define EI_MAG3    3
#define EI_CLASS   4
#define EI_DATA    5
#define EI_VERSION 6
#define EI_OSABI   7

/* ELF magic bytes */
#define ELFMAG0 0x7F
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

/* 32-bit objects */
#define ELFCLASS32 1

/* Little endian */
#define ELFDATA2LSB 1

/* Machine types */
#define EM_386 3
#define EM_486 6

/* ELF types */
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3

/* Flags */
#define PF_X 1
#define PF_W 2
#define PF_R 4

/* Forward declaration */
struct vfs_node;
struct task;

/* ELF load result */
typedef struct {
    int success;
    uint32_t entry_point;
    uint32_t load_addr;
    uint32_t stack_size;
} elf_load_result_t;

int elf_validate(elf_header_t *header);
elf_load_result_t elf_load(struct vfs_node *node);
int elf_load_task(struct vfs_node *node, const char *name);

#endif

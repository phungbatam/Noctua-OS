ENTRY(_start)

SECTIONS {
    . = 1M;

    .multiboot BLOCK(4K) : ALIGN(4K) {
        *(.multiboot)
    }

    .text BLOCK(4K) : ALIGN(4K) {
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K) {
        *(.rodata)
    }

    .initcall : ALIGN(4) {
        __start_initcall0_init = .;
        *(.initcall0.init)
        __stop_initcall0_init = .;
        __start_initcall1_init = .;
        *(.initcall1.init)
        __stop_initcall1_init = .;
        __start_initcall2_init = .;
        *(.initcall2.init)
        __stop_initcall2_init = .;
        __start_initcall3_init = .;
        *(.initcall3.init)
        __stop_initcall3_init = .;
        __start_initcall4_init = .;
        *(.initcall4.init)
        __stop_initcall4_init = .;
        __start_initcall5_init = .;
        *(.initcall5.init)
        __stop_initcall5_init = .;
        __start_initcall6_init = .;
        *(.initcall6.init)
        __stop_initcall6_init = .;
        __start_initcall7_init = .;
        *(.initcall7.init)
        __stop_initcall7_init = .;
    }

    .data BLOCK(4K) : ALIGN(4K) {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K) {
        *(COMMON)
        *(.bss)
    }

    _end = ALIGN(4K);
}

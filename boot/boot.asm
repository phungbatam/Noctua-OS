section .multiboot
align 4
    dd 0x1BADB002
    dd 0x05
    dd -(0x1BADB002 + 0x05)
    dd 0
    dd 640
    dd 400
    dd 32

section .text
global _start
extern kmain

_start:
    mov esp, stack_top
    push ebx
    call kmain

    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 32768
stack_top:
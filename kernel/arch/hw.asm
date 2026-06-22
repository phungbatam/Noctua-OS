section .text
global enter_user_mode
global syscall_entry
extern syscall_handler

; enter_user_mode(uint32_t eip, uint32_t esp, uint32_t stack_seg, uint32_t code_seg)
enter_user_mode:
    push ebp
    mov ebp, esp

    mov ax, [ebp + 16]     ; stack_seg (user data segment, ring 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push dword [ebp + 16]  ; SS
    push dword [ebp + 12]  ; ESP
    pushf                  ; EFLAGS
    pop eax
    or eax, 0x200          ; enable IF (interrupts)
    push eax               ; EFLAGS back
    push dword [ebp + 20]  ; CS
    push dword [ebp + 8]   ; EIP
    iret

; syscall_entry - called from int 0x80
; Stack state at entry: SS, ESP, EFLAGS, CS, EIP (from interrupt gate)
; We want to call syscall_handler(eax, ebx, ecx, edx, esi, edi)
; For ring 3 -> ring 0 transition, SS and ESP are automatically saved
syscall_entry:
    push ds
    push es
    push fs
    push gs

    push edi
    push esi
    push edx
    push ecx
    push ebx

    mov ax, 0x10           ; kernel data segment
    mov ds, ax
    mov es, ax

    push esp               ; push registers pointer
    call syscall_handler
    add esp, 4

    pop ebx
    pop ecx
    pop edx
    pop esi
    pop edi

    pop gs
    pop fs
    pop es
    pop ds

    iret

; Raw atomic operations (backup for atomic.h inline asm)
global atomic_xchg_raw
global atomic_cmpxchg_raw

atomic_xchg_raw:
    mov ecx, [esp + 4]
    mov eax, [esp + 8]
    xchg [ecx], eax
    ret

atomic_cmpxchg_raw:
    mov ecx, [esp + 4]
    mov eax, [esp + 8]
    mov edx, [esp + 12]
    lock cmpxchg [ecx], edx
    ret

; Optimized memory operations
global fast_memcpy
global fast_memset

fast_memcpy:
    push esi
    push edi

    mov edi, [esp + 12]    ; dest
    mov esi, [esp + 16]    ; src
    mov ecx, [esp + 20]    ; len

    ; Align to 4 bytes for faster copy
    mov edx, edi
    and edx, 3
    jz .aligned

    ; Copy bytes until aligned
    neg edx
    add edx, 4
    cmp ecx, edx
    jb .byte_copy
    sub ecx, edx
.align_loop:
    movsb
    dec edx
    jnz .align_loop

.aligned:
    ; Copy 4 bytes at a time
    mov edx, ecx
    and ecx, 3
    shr edx, 2
    rep movsd
    mov ecx, edx

.byte_copy:
    rep movsb
    mov eax, [esp + 12]    ; return dest
    pop edi
    pop esi
    ret

fast_memset:
    push edi
    mov edi, [esp + 8]     ; dest
    mov eax, [esp + 12]    ; value
    mov ecx, [esp + 16]    ; len

    ; Fill byte by byte
    rep stosb
    mov eax, [esp + 8]     ; return dest
    pop edi
    ret

; I/O wait
global io_wait
io_wait:
    mov dx, 0x80
    out dx, al
    ret

; Read CPU timestamp counter
global read_tsc
read_tsc:
    rdtsc
    ret                  ; returns edx:eax in eax (low 32 bits)

; Halt CPU
global halt_cpu
halt_cpu:
    hlt
    ret

; Interrupt enable/disable
global cli_custom
global sti_custom
cli_custom:
    cli
    ret
sti_custom:
    sti
    ret

global start
extern kernel_main

MAGIC     equ 0x1BADB002
FLAGS     equ 3
CHECKSUM  equ -(MAGIC + FLAGS)
CODE_SEG  equ gdt_code - gdt_start
DATA_SEG  equ gdt_data - gdt_start

section .note.GNU-stack noalloc noexec nowrite progbits

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .stack nobits alloc
align 16
stack_bottom:
    resb 16384      ; 16 KiB stack
stack_top:

section .text
bits 32

start:
    cli
    lgdt [gdt_descriptor]
    jmp CODE_SEG:flush_segments

flush_segments:
    mov cx, DATA_SEG
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx
    mov ss, cx

    mov esp, stack_top  ; set up stack before doing anything
    push ebx;
    push eax;
    call kernel_main

hang:
    cli
    hlt
    jmp hang

section .rodata
align 8
gdt_start:
gdt_null:
    dq 0
gdt_code:
    dq 0x00CF9A000000FFFF
gdt_data:
    dq 0x00CF92000000FFFF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

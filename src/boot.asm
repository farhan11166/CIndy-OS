global start
extern kernel_main

MAGIC     equ 0x1BADB002
FLAGS     equ 0
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
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, stack_top  ; set up stack before doing anything
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

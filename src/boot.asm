global start
extern kernel_main

global gdt_tss
global stack_top
MAGIC     equ 0x1BADB002
FLAGS     equ 3
CHECKSUM  equ -(MAGIC + FLAGS)
KERNEL_CODE_SEG equ gdt_code - gdt_start
KERNEL_DATA_SEG equ gdt_data - gdt_start
USER_CODE_SEG   equ gdt_user_code - gdt_start
USER_DATA_SEG   equ gdt_user_data - gdt_start

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
    ; GRUB passes the multiboot magic in EAX and the info pointer in EBX.
    ; We must preserve those values before we reload segment registers or touch
    ; EAX/EBX in the setup below.
    mov esi, eax
    mov edi, ebx

    lgdt [gdt_descriptor]
    jmp KERNEL_CODE_SEG:flush_segments

flush_segments:
    mov ax, KERNEL_DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, stack_top  ; set up stack before doing anything
    push edi
    push esi
    call kernel_main

hang:
    cli
    hlt
    jmp hang
section .data
align 8
gdt_start:
gdt_null:
    dq 0
gdt_code:
    dq 0x00CF9A000000FFFF
gdt_data:
    dq 0x00CF92000000FFFF
gdt_user_code:
    dq 0x00CFFA000000FFFF
gdt_user_data:
    dq 0x00CFF2000000FFFF
gdt_tss:
    dq 0
    dq 0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start
   

; Multiboot header constants
MBALIGN  equ 1 << 0              ; align loaded modules on page boundaries
MEMINFO  equ 1 << 1              ; provide memory map
FLAGS    equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002          ; multiboot magic number
CHECKSUM equ -(MAGIC + FLAGS)    ; checksum to prove we are multiboot

; Multiboot header – must be in the first 8 KiB of the kernel binary
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; Small initial stack (16 KiB)
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

; Kernel entry point
section .text
global _start
extern kernel_main

_start:
    ; Set up the stack
    mov esp, stack_top

    ; Call the C kernel
    call kernel_main

    ; Halt forever if kernel_main returns
.hang:
    cli
    hlt
    jmp .hang

; Mark stack as non-executable (suppresses linker warning)
section .note.GNU-stack noalloc noexec nowrite progbits

global isr33
extern keyboard_handler

section .text
bits 32

isr33:
    cli
    pusha

    call keyboard_handler

    popa
    iret

section .note.GNU-stack noalloc noexec nowrite progbits

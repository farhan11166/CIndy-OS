global isr33
global isr32
extern keyboard_handler
extern timer_handler

section .text
bits 32
isr32:
    cli
    pusha

    call timer_handler
    popa
    iret
isr33:
    cli
    pusha

    call keyboard_handler

    popa
    iret

section .note.GNU-stack noalloc noexec nowrite progbits

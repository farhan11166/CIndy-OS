global enable_interrupts

section .text
bits 32

enable_interrupts:
    sti
    ret

section .note.GNU-stack noalloc noexec nowrite progbits

global tss_flush

section .text
bits 32

tss_flush:
  mov ax, [esp+4] ; selector passed as argument
  ltr ax          ; Load Task Register
  ret

section .note.GNU-stack noalloc noexec nowrite progbits
  

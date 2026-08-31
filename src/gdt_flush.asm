global tss_flush

section .text
bits 32

tss_flush:
  ; Keep this explicit and independent from the C stack so the TSS is loaded
  ; reliably before any ring-3 transition occurs.
  mov ax, 0x28
  ltr ax
  ret

section .note.GNU-stack noalloc noexec nowrite progbits
  

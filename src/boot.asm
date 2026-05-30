; ── Multiboot header ──────────────────────────────────────────────────────────
MBALIGN  equ 1 << 0
MEMINFO  equ 1 << 1
FLAGS    equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; ── Small initial stack (16 KiB) ───────────────────────────────────────────────
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

; ── Entry point ────────────────────────────────────────────────────────────────
section .text
bits 32                   ; <-- was "bit 32" (typo)
global _start
extern kernel_main

_start:
    mov esp, stack_top    ; set up the stack
    call kernel_main      ; jump into C

hang:
    cli
    hlt
    jmp hang              ; loop forever if kernel returns
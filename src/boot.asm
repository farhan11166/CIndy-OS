global start
extern kernel_main

MAGIC     equ 0x1BADB002
FLAGS     equ 0
CHECKSUM  equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .text
bits 32

start:
    call kernel_main

hang:
    jmp hang
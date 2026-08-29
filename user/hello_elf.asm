[BITS 32]
[GLOBAL _start]

section .text
_start:
    ; SYS_WRITE(stdout, msg, len)
    mov eax, 1
    mov ebx, 1
    mov ecx, msg
    mov edx, msg_len
    int 0x80

    ; SYS_EXIT(0)
    xor ebx, ebx
    mov eax, 0
    int 0x80

section .rodata
msg: db "Hello from ELF user mode!\n", 0
msg_len equ $ - msg

[BITS 32]
[ORG 0x40000000]

; code comes first — CPU jumps here on entry
_start:
    ; SYS_WRITE: eax=1, ebx=fd(1=stdout), ecx=msg ptr, edx=len
    mov eax, 1
    mov ebx, 1
    mov ecx, msg
    mov edx, msg_len
    int 0x80

    ; SYS_EXIT: eax=0
    mov eax, 0
    xor ebx, ebx
    int 0x80

; data after code so it doesn't get executed
msg:    db "HELLO FROM USER MODE!", 0x0A
msg_len equ $ - msg

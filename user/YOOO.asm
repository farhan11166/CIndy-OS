[BITS 32]

msg: db "HELLO FROM USER MODE!\n"
msg_len equ $-msg

; SYS_WRITE: eax = 1, ebx = fd, ecx = pointer, edx = len
mov eax, 1
mov ebx, 1
mov ecx, msg
mov edx, msg_len
int 0x80

; SYS_EXIT: eax = 0
mov eax, 0
int 0x80

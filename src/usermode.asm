global jump_to_usermode
global kernel_return
extern saved_kernel_esp

USER_CODE_SEG equ 0x1b
USER_DATA_SEG equ 0x23

section .text
bits 32

; void jump_to_usermode(uint32_t user_stack, uint32_t user_entry)
jump_to_usermode:
    mov eax, [esp+4]   ; user_stack
    mov ecx, [esp+8]   ; user_entry

    ; Save the kernel's esp so kernel_return can come back here.
    ; kernel_return does `mov esp, [saved_kernel_esp]; ret`, so we must
    ; store the actual esp value (pointing at the return address),
    ; NOT esp+4 (which would dereference into the argument list).
    mov edx, esp
    mov [saved_kernel_esp], edx

    cli

    ; build iret frame
    push USER_DATA_SEG  ; ss
    push eax            ; esp (user stack top)
    pushf               ; eflags
    pop  eax
    or   eax, 0x200     ; ensure IF set
    push eax
    push USER_CODE_SEG  ; cs
    push ecx            ; eip

    ; switch data segment registers to ring-3 before iret
    mov ax, USER_DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    iret

; Called from syscall_handler on exit syscall.
; Restores kernel segments and stack, then returns to the C caller
; of jump_to_usermode (i.e. the run command in keyboard.c).
kernel_return:
    ; restore kernel data segments
    mov ax, 0x10        ; KERNEL_DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; restore kernel stack to the value saved before iret
    mov esp, [saved_kernel_esp]

    ; return to whoever called jump_to_usermode()
    ret

section .note.GNU-stack noalloc noexec nowrite progbits



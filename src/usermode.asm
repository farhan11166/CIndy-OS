global jump_to_usermode
global kernel_return
extern saved_kernel_esp
extern tss_set_kernel_stack

USER_CODE_SEG equ 0x1b
USER_DATA_SEG equ 0x23

section .text
bits 32

; void jump_to_usermode(uint32_t user_stack, uint32_t user_entry)
jump_to_usermode:
    mov eax, [esp+4]   ; user_stack
    mov ecx, [esp+8]   ; user_entry

    ; Update TSS esp0 to the current kernel esp so ring-3 interrupts build
    ; their kernel-mode frame here (growing down), not from the fixed stack_top
    ; which would collide with live kernel call frames.
    push eax            ; preserve user_stack          [esp = entry_esp - 4]
    push ecx            ; preserve user_entry          [esp = entry_esp - 8]
    push esp            ; push current esp value       [esp = entry_esp - 12]
    ; [esp] now holds (entry_esp - 8).  We want TSS esp0 = entry_esp so that
    ; the CPU's ring-3 -> ring-0 frame (ss,esp,eflags,cs,eip = 5 dwords) is
    ; pushed at entry_esp-4 downward, leaving the return address at entry_esp
    ; completely untouched.  Add 8 to recover entry_esp from (entry_esp - 8).
    mov edx, [esp]      ; edx = entry_esp - 8
    add edx, 8          ; edx = entry_esp  (NOT +12, which would be entry_esp+4
                        ;   and would let ss_user overwrite the return address)
    mov [esp], edx      ; patch the argument
    call tss_set_kernel_stack
    add  esp, 4         ; remove arg
    pop  ecx            ; restore user_entry
    pop  eax            ; restore user_stack

    ; Save kernel esp NOW (after stack is balanced) so kernel_return ret works.
    ; At this point esp == entry esp, pointing at the return address.
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

    ; re-enable interrupts — they were cleared by cli in jump_to_usermode and
    ; are never restored via the normal isr128 iret path on SYS_EXIT
    sti

    ; return to whoever called jump_to_usermode()
    ret

section .note.GNU-stack noalloc noexec nowrite progbits



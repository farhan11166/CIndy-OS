global isr33
global isr32
extern keyboard_handler
extern timer_handler
extern fault_handler

section .text
bits 32
; This macro creates a stub for an exception that DOES NOT push an error code
isr_common_stub:
   pusha

   mov ax,ds
   push eax

   mov ax,0x10
   mov ds,ax
   mov es,ax
   mov fs,ax
   mov gs,ax
   mov ss,ax

   ; push the stack pointer
   push esp

   call fault_handler

   add esp,4

   pop eax
   mov ds,ax
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax
   popa                ; Pops edi, esi, ebp...
   add esp, 8          ; Cleans up the pushed error code and pushed ISR number
   sti
   iret 

   
%macro ISR_NOERRCODE 1
  global isr%1
  isr%1:
    cli
    push byte 0
    push byte %1
    jmp isr_common_stub
%endmacro


; This macro creates a stub for an exception that DOES push an error code
%macro ISR_ERRCODE 1
  global isr%1
  isr%1:
    cli
    ; The CPU already pushed the error code
    push byte %1 ; Push the interrupt number
    jmp isr_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE 8
ISR_NOERRCODE 9
ISR_ERRCODE 10
ISR_ERRCODE 11
ISR_ERRCODE 12
ISR_ERRCODE 13
ISR_ERRCODE 14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

isr32:
    cli
    pusha

    call timer_handler
    popa
    iret
isr33:
    cli
    pusha

    call keyboard_handler

    popa
    iret

section .note.GNU-stack noalloc noexec nowrite progbits

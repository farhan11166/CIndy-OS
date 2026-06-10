#include "../include/screen.h"
#include "../include/ports.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/interrupts.h"
#include "../include/timer.h"
#include "../include/keyboard.h"
extern void isr33();
extern void isr32();
extern void timer_handler();


void keyboard_handler() {
    unsigned char scancode = inb(0x60);
    handle_keyboard_interrupt(scancode);
    outb(0x20, 0x20);   // EOI — must send before returning
}

void kernel_main() {

    clear_screen();

    print("Welcome to CIndy-OS\n");
    print("Boot successful\n");
    print("Kernel loaded successfully\n");
    print("Integer test: ");
    print_int(12345);

    print("\n");

    print("Hex test: ");
    print_hex(3735928559);
    print("\n");
    print("Testing ports");
    print("\n");
    print("Initializing IDT...\n");

    idt_init();

    print("IDT loaded successfully\n");
    

    print("\n");
    print("Remapping PIC...\n");

    pic_remap();

    print("PIC remapped successfully\n");
    print("Enabling interrupts...\n");
    init_timer(100);
    idt_set_gate(32, (unsigned int)isr32, 0x08, 0x8E);
    idt_set_gate(33, (unsigned int)isr33, 0x08, 0x8E);
    enable_interrupts();

    print("Interrupts enabled.\n");
    print("[CIndy-OS]> ");
    while(1){
        asm volatile("hlt");
    }
    
}

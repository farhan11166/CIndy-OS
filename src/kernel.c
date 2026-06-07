#include "../include/screen.h"
#include "../include/ports.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/interrupts.h"

extern void isr33();

volatile unsigned char last_scancode = 0;
volatile int key_pressed = 0;

void keyboard_handler() {
    last_scancode = inb(0x60);
    key_pressed = 1;
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
    idt_set_gate(33, (unsigned int)isr33, 0x08, 0x8E);
    enable_interrupts();

    print("Interrupts enabled.\n");
     while (1) {
        asm volatile("hlt");
        if (key_pressed) {
            key_pressed = 0;
            print("KEY: ");
            print_hex(last_scancode);
            print("\n");
        }
    }
    
}

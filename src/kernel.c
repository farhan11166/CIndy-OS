#include "../include/screen.h"
#include "../include/ports.h"
#include "../include/idt.h"
#include "../include/pic.h"
void keyboard_handler() {
    print("KEYBOARD INTERRUPT\n");
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
}

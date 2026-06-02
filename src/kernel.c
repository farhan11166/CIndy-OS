#include "../include/screen.h"
#include "../include/ports.h"

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
    outb(0x3F8,1);
    unsigned char port_val = inb(0x60);
    print_int(port_val);
    

    print("\n");
}

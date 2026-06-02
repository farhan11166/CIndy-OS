#include "../include/screen.h"

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
}

#include "../include/screen.h"
#include "../include/ports.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/interrupts.h"
#include "../include/timer.h"
#include "../include/keyboard.h"
#include "../include/multiboot.h"
#include "../include/fs.h"
#include "../include/memory.h"
#include "../include/fat16.h"
extern void isr0(); extern void isr1(); extern void isr2(); extern void isr3();
extern void isr4(); extern void isr5(); extern void isr6(); extern void isr7();
extern void isr8(); extern void isr9(); extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();


extern void isr33();
extern void isr32();
extern void timer_handler();


void keyboard_handler() {
    unsigned char scancode = inb(0x60);
    handle_keyboard_interrupt(scancode);
    outb(0x20, 0x20);   // EOI — must send before returning
}

void kernel_main(unsigned int magic, struct multiboot_info* mbd) {

    clear_screen();
    enable_cursor(14, 15);
    print_colored("   ____ ___           _           ___  ____  \n", 0x0A);
    print_colored("  / ___|_ _|____   __| |_   _    / _ \\/ ___| \n", 0x0A);
    print_colored(" | |    | | '_ \\ \\ / / | | | |  | | | \\___ \\ \n", 0x0A);
    print_colored(" | |___ | | | | \\ V /| | |_| |  | |_| |___) |\n", 0x0A);
    print_colored("  \\____|___|_| |_|\\_/ |_|\\__, |  \\___/|____/ \n", 0x0A);
    print_colored("                         |___/               \n", 0x0A);
    print("\n");

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

    // Register CPU Exceptions (0-31)
    idt_set_gate(0, (unsigned int)isr0, 0x08, 0x8E);
    idt_set_gate(1, (unsigned int)isr1, 0x08, 0x8E);
    idt_set_gate(2, (unsigned int)isr2, 0x08, 0x8E);
    idt_set_gate(3, (unsigned int)isr3, 0x08, 0x8E);
    idt_set_gate(4, (unsigned int)isr4, 0x08, 0x8E);
    idt_set_gate(5, (unsigned int)isr5, 0x08, 0x8E);
    idt_set_gate(6, (unsigned int)isr6, 0x08, 0x8E);
    idt_set_gate(7, (unsigned int)isr7, 0x08, 0x8E);
    idt_set_gate(8, (unsigned int)isr8, 0x08, 0x8E);
    idt_set_gate(9, (unsigned int)isr9, 0x08, 0x8E);
    idt_set_gate(10, (unsigned int)isr10, 0x08, 0x8E);
    idt_set_gate(11, (unsigned int)isr11, 0x08, 0x8E);
    idt_set_gate(12, (unsigned int)isr12, 0x08, 0x8E);
    idt_set_gate(13, (unsigned int)isr13, 0x08, 0x8E);
    idt_set_gate(14, (unsigned int)isr14, 0x08, 0x8E);
    idt_set_gate(15, (unsigned int)isr15, 0x08, 0x8E);
    idt_set_gate(16, (unsigned int)isr16, 0x08, 0x8E);
    idt_set_gate(17, (unsigned int)isr17, 0x08, 0x8E);
    idt_set_gate(18, (unsigned int)isr18, 0x08, 0x8E);
    idt_set_gate(19, (unsigned int)isr19, 0x08, 0x8E);
    idt_set_gate(20, (unsigned int)isr20, 0x08, 0x8E);
    idt_set_gate(21, (unsigned int)isr21, 0x08, 0x8E);
    idt_set_gate(22, (unsigned int)isr22, 0x08, 0x8E);
    idt_set_gate(23, (unsigned int)isr23, 0x08, 0x8E);
    idt_set_gate(24, (unsigned int)isr24, 0x08, 0x8E);
    idt_set_gate(25, (unsigned int)isr25, 0x08, 0x8E);
    idt_set_gate(26, (unsigned int)isr26, 0x08, 0x8E);
    idt_set_gate(27, (unsigned int)isr27, 0x08, 0x8E);
    idt_set_gate(28, (unsigned int)isr28, 0x08, 0x8E);
    idt_set_gate(29, (unsigned int)isr29, 0x08, 0x8E);
    idt_set_gate(30, (unsigned int)isr30, 0x08, 0x8E);
    idt_set_gate(31, (unsigned int)isr31, 0x08, 0x8E);

    print("IDT loaded successfully\n");
    

    print("\n");
    print("Remapping PIC...\n");

    pic_remap();

    print("PIC remapped successfully\n");
    print("Enabling interrupts...\n");
    
    idt_set_gate(32, (unsigned int)isr32, 0x08, 0x8E);
    idt_set_gate(33, (unsigned int)isr33, 0x08, 0x8E);
    enable_interrupts();
    init_timer(100);
    
    //int a=1/0;
    init_memory(mbd,magic);
    if(mbd->mods_count >0){
        struct multiboot_mod_list* module=(struct multiboot_mod_list*)mbd->mods_addr;
        init_fs(module->mod_start);
    }
    else{
        print("Warning: No initrd module loaded by GRUB!\n");

    }
    fat16_init();

    print("Interrupts enabled.\n");
    print_colored("\n[CIndy-OS]> ", 0x0B);
    
    while(1){
        asm volatile("hlt");
    }
    

    
}

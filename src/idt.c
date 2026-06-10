#include "../include/idt.h"
#include "../include/screen.h"
struct idt_entry idt[256];
struct idt_ptr idtp;



void idt_init() {

    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt;
    idt_load(&idtp);
}
void idt_set_gate(
    unsigned char num,
    unsigned int base,
    unsigned short selector,
    unsigned char flags
) {

    idt[num].base_low = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;

    idt[num].selector = selector;
    idt[num].always_zero = 0;
    idt[num].flags = flags;
}
char *exception_messages[] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Into Detected Overflow", "Out of Bounds", "Invalid Opcode", "No Coprocessor",
    "Double Fault", "Coprocessor Segment Overrun", "Bad TSS", "Segment Not Present",
    "Stack Fault", "General Protection Fault", "Page Fault", "Unknown Interrupt",
    "Coprocessor Fault", "Alignment Check", "Machine Check", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved"
};
void fault_handler(struct registers *regs) {
    // If the interrupt number is less than 32, it's a CPU exception
    if (regs->int_no < 32) {
        print("EXCEPTION: ");
        print(exception_messages[regs->int_no]);
        print("\n");
        print("System Halted!\n");
        
        // Halt the CPU forever
        while(1) {
            asm volatile("cli; hlt");
        }
    }
}


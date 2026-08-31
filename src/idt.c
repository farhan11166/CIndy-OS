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
    if (regs->int_no < 32) {
        print("EXCEPTION: ");
        print(exception_messages[regs->int_no]);
        print("\n");

        print("  EIP: "); print_hex(regs->eip);
        print("  CS:  "); print_hex(regs->cs);
        print("\n");
        print("  ERR: "); print_hex(regs->err_code);
        print("\n");

        // For page faults (int 14), CR2 holds the faulting virtual address
        if (regs->int_no == 14) {
            unsigned int cr2;
            asm volatile("mov %%cr2, %0" : "=r"(cr2));
            print("  CR2 (fault addr): "); print_hex(cr2);
            print("\n");
            // agent_dbg("B", "idt.c:fault_handler", "page_fault", cr2, regs->eip, regs->err_code, regs->cs);

            // Decode error code bits
            print("  Cause: ");
            if (regs->err_code & 1) print("protection violation"); else print("not present");
            if (regs->err_code & 2) print(", write"); else print(", read");
            if (regs->err_code & 4) print(", user mode"); else print(", kernel mode");
            print("\n");
        }

        print("System Halted!\n");
        while(1) { asm volatile("cli; hlt"); }
    }
}


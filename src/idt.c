#include "../include/idt.h"

struct idt_entry idt[256];
struct idt_ptr idtp;
void idt_init() {

    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt;
    idt_set_gate(
    33,
    (unsigned int)isr33,
    0x08,
    0x8E
    );
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
    idt[num].zero = 0;
    idt[num].flags = flags;
}
extern void isr33();
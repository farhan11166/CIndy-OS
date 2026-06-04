#include "../include/idt.h"

struct idt_entry idt[256];
struct idt_ptr idtp;
void idt_init() {

    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt;

}
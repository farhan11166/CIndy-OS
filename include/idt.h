#ifndef IDT_H
#define IDT_H

struct idt_entry{
    unsigned short base_low;
    unsigned short selector;
    unsigned char always_zero;
    unsigned char flags;
    unsigned short base_high;
}__attribute__((packed));

struct idt_ptr{
    unsigned short limit;
    unsigned int base;
}__attribute__((packed));
void idt_set_gate(
    unsigned char num,
    unsigned int base,
    unsigned short selector,
    unsigned char flags
);

void idt_init();
struct registers {
    unsigned int ds;                                     
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;  
    unsigned int int_no, err_code;                        
    unsigned int eip, cs, eflags, useresp, ss;           
};
extern void idt_load(struct idt_ptr* idtp);

#endif
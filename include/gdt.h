#ifndef GDT_H
#define GDT_H
#include "./types.h"
#define KERNEL_CODE_SEG 0x08
#define KERNEL_DATA_SEG 0x10
#define USER_CODE_SEG   0x1B
#define USER_DATA_SEG   0x23
#define TSS_SEG         0x28

void gdt_init_tss(uint32_t kernel_stack_top);
void tss_set_kernel_stack(uint32_t stack);
void kernel_return();
void write_tss_descriptor(uint32_t base, uint32_t limit);
void jump_to_usermode(uint32_t user_stack, uint32_t user_entry);

extern uint32_t saved_kernel_esp;



#endif 
#ifndef MEMORY_H
#define MEMORY_H
#include "multiboot.h"
#include "types.h"
void init_memory(struct multiboot_info* mbd,unsigned int magic);
void* kmalloc(unsigned int size);
void* kmalloc_a(unsigned int size);
void init_pmm(struct multiboot_info* mbd);
uint32_t pmm_alloc_block(void);
void pmm_free_block(uint32_t p_addr);

unsigned int get_total_memory();
unsigned int get_allocated_memory();

#endif

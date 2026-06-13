#ifndef MEMORY_H
#define MEMORY_H
#include "multiboot.h"
void init_memory(struct multiboot_info* mbd,unsigned int magic);
void* kmalloc(unsigned int size);
unsigned int get_total_memory();
unsigned int get_allocated_memory();

#endif

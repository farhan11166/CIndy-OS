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
/* Mark every 4KB frame covered by [phys_addr, phys_addr+size) as used in
 * the PMM bitmap.  Call this after any kmalloc_a() allocation that the PMM
 * would otherwise hand out again (because kmalloc_a does not update the
 * bitmap itself). */
void pmm_mark_used(uint32_t phys_addr, uint32_t size);

unsigned int get_total_memory();
unsigned int get_allocated_memory();

#endif

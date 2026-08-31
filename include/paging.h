#ifndef PAGING_H
#define PAGING_H
#include "types.h"

void init_paging();
void map_page(uint32_t physical_addr, uint32_t virtual_addr, uint32_t flags);
int validate_user_region(uint32_t user_addr, uint32_t len);
uint32_t get_pte(uint32_t virtual_addr);

#endif

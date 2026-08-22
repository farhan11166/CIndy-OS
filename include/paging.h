#ifndef PAGING_H
#define PAGING_H
#include "types.h"

void init_paging();
void map_page(uint32_t physical_addr, uint32_t virtual_addr, uint32_t flags);

#endif

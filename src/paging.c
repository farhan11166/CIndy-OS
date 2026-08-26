#include "../include/paging.h"
#include "../include/memory.h"
#include "../include/types.h"

uint32_t* page_direc;

void init_paging() {
    // Allocate the page directory (1024 entries x 4 bytes = 4KB, page-aligned)
    page_direc = (uint32_t*)kmalloc_a(1024 * 4);

    // Clear all entries — nothing mapped yet
    for (int i = 0; i < 1024; i++) {
        page_direc[i] = 0;
    }

    // Identity-map ALL of physical RAM so that:
    //   1. The kernel keeps working after paging is enabled (virtual == physical)
    //   2. map_page() can safely dereference a physical address as a virtual pointer
    //      when writing into a newly allocated page table — it is always in this range.
    //
    // We cover 128MB (32 page tables x 4MB each).
    // Each page table: 1024 pages x 4KB = 4MB.
    // Total cost: 32 x 4KB = 128KB of RAM for tables.
    #define IDENTITY_MAP_TABLES 32

    for (int t = 0; t < IDENTITY_MAP_TABLES; t++) {
        uint32_t* table = (uint32_t*)kmalloc_a(1024 * 4);

        for (int p = 0; p < 1024; p++) {
            uint32_t phys = ((uint32_t)t * 1024 + (uint32_t)p) * 0x1000;
            table[p] = phys | 7; // present + read/write + user
        }

        page_direc[t] = (uint32_t)table | 7; // present + read/write + user
    }

    // Load page directory base into CR3
    asm volatile("mov %0, %%cr3" :: "r"(page_direc));

    // Set bit 31 of CR0 to enable paging
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

void map_page(uint32_t physical_addr, uint32_t virtual_addr, uint32_t flags) {
    // Virtual address layout: [pd_index:10][pt_index:10][offset:12]
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;

    if (!(page_direc[pd_index] & 1)) {
        // No page table exists for this PDE yet — allocate one.
        uint32_t new_table_phy = pmm_alloc_block();

        // Install in the page directory
        page_direc[pd_index] = new_table_phy | 7;

        // Because init_paging() identity-maps all of RAM, new_table_phy is a
        // valid virtual address here — physical == virtual in the identity map.
        uint32_t* page_table = (uint32_t*)new_table_phy;
        for (int i = 0; i < 1024; i++) {
            page_table[i] = 0;
        }
        page_table[pt_index] = physical_addr | flags | 1;

    } else {
        // Page table already exists — physical address == virtual (identity map)
        uint32_t* page_table = (uint32_t*)(page_direc[pd_index] & 0xFFFFF000);
        page_table[pt_index] = physical_addr | flags | 1;
    }

    // Flush the TLB entry for this virtual address
    asm volatile("invlpg (%0)" :: "r"(virtual_addr) : "memory");
}

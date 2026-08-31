#include "../include/paging.h"
#include "../include/memory.h"
#include "../include/types.h"
#include "../include/screen.h"

uint32_t* page_direc;

void init_paging() {
    // Allocate the page directory (1024 entries x 4 bytes = 4KB, page-aligned)
    page_direc = (uint32_t*)kmalloc_a(1024 * 4);

    // Clear all entries — nothing mapped yet
    for (int i = 0; i < 1024; i++) {
        page_direc[i] = 0;
    }

    // Identity-map enough of physical RAM to cover the kernel, page tables,
    // and any user-space pages we allocate dynamically. The old version only
    // covered 128MB, which is too small once page tables themselves are
    // allocated outside that range.
    uint32_t mapped_bytes = get_total_memory();
    if (mapped_bytes == 0) {
        mapped_bytes = 0x20000000u; // 512MB fallback if memory detection failed
    }
    uint32_t required_pdes = (mapped_bytes + 0x3FFFFFu) / 0x400000u;
    if (required_pdes > 1024) {
        required_pdes = 1024;
    }

    for (uint32_t t = 0; t < required_pdes; t++) {
        uint32_t* table = (uint32_t*)kmalloc_a(1024 * 4);

        for (uint32_t p = 0; p < 1024; p++) {
            uint32_t phys = ((uint32_t)t * 1024 + p) * 0x1000u;
            table[p] = phys | 7u; // present + read/write + user
        }

        page_direc[t] = (uint32_t)table | 7u; // present + read/write + user
    }

    // Load page directory base into CR3
    asm volatile("mov %0, %%cr3" :: "r"(page_direc));

    // Set bit 31 of CR0 to enable paging
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

int validate_user_region(uint32_t user_addr, uint32_t len) {
    if (len == 0) return 1;

    /* User ELF binaries normally start below 3 GiB (e.g. 0x08048000), while the
     * kernel uses the upper memory region.  The old restriction to only
     * 0x40000000..0xC0000000 rejected valid ELF user images and caused user-mode
     * faults during the `run` flow. */
    if (user_addr < 0x100000u || user_addr >= 0xC0000000u) return 0;

    uint32_t end = user_addr + len;
    if (end < user_addr || end > 0xC0000000u) return 0;

    for (uint32_t addr = user_addr; addr < end; addr += 0x1000u) {
        uint32_t pd_index = addr >> 22;
        uint32_t pt_index = (addr >> 12) & 0x3FFu;

        if (pd_index >= 1024) return 0;
        uint32_t pde = page_direc[pd_index];
        if ((pde & 1u) == 0u) return 0;
        uint32_t* page_table = (uint32_t*)(pde & 0xFFFFF000u);
        uint32_t pte = page_table[pt_index];
        if ((pte & 1u) == 0u) return 0;
        if ((pte & 0x4u) == 0u) return 0; // user bit must be set
    }

    return 1;
}

void map_page(uint32_t physical_addr, uint32_t virtual_addr, uint32_t flags) {
    // Virtual address layout: [pd_index:10][pt_index:10][offset:12]
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;

    if (!(page_direc[pd_index] & 1)) {
        // No page table exists for this PDE yet.
        // Use pmm_alloc_block() so the page table frame comes from the same
        // pool as all other physical allocations.  kmalloc_a() must NOT be
        // used here because pmm_alloc_block() does not know about memory
        // handed out by kmalloc_a() after init_pmm() ran, which would cause
        // the PMM to hand out the same frame later (e.g. for a user stack),
        // overwriting the page table with zeroes and silently unmapping pages.
        uint32_t pt_phys = pmm_alloc_block();
        if (pt_phys == 0) {
            // Out of physical memory — cannot create page table
            print("map_page: pmm_alloc_block failed for page table\n");
            return;
        }

        // The PMM frame is in the identity-mapped region (phys == virt).
        uint32_t* page_table = (uint32_t*)pt_phys;

        // Install the new page table in the directory.
        page_direc[pd_index] = pt_phys | 7;

        // Zero the new page table before filling it.
        for (int i = 0; i < 1024; i++) {
            page_table[i] = 0;
        }
        page_table[pt_index] = physical_addr | flags | 1;

        /* Uncomment to debug page table creation:
        print("map_page: created page table for PD index ");
        print_hex(pd_index);
        print(", mapped vaddr 0x");
        print_hex(virtual_addr);
        print(" -> phys 0x");
        print_hex(physical_addr);
        print(" PDE=0x"); print_hex(page_direc[pd_index]);
        print(" PTE=0x"); print_hex(page_table[pt_index]);
        print("\n");
        */

    } else {
        // Page table already exists — physical address == virtual (identity map)
        uint32_t* page_table = (uint32_t*)(page_direc[pd_index] & 0xFFFFF000);
        page_table[pt_index] = physical_addr | flags | 1;

        /* Uncomment to debug existing table mappings:
        print("map_page: mapped vaddr 0x");
        print_hex(virtual_addr);
        print(" -> phys 0x");
        print_hex(physical_addr);
        print(" PDE=0x"); print_hex(page_direc[pd_index]);
        print(" PTE=0x"); print_hex(page_table[pt_index]);
        print("\n");
        */
    }

    // Flush the TLB entry for this virtual address
    asm volatile("invlpg (%0)" :: "r"(virtual_addr) : "memory");
}

uint32_t get_pte(uint32_t virtual_addr) {
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;
    if (!(page_direc[pd_index] & 1)) {
        return 0;
    }
    uint32_t *page_table = (uint32_t *)(page_direc[pd_index] & 0xFFFFF000);
    return page_table[pt_index];
}

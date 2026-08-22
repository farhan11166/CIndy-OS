#include "../include/memory.h"
#include "../include/screen.h"
#include "../include/types.h"
unsigned int free_mem_addr= 0x200000;
unsigned int total_memory=0;

unsigned int allocated_memory=0;
uint32_t mem_bitmap[8192];
uint32_t max_frames = 0;

void  init_memory(struct multiboot_info* mbd, unsigned int magic){
    print("Init Mem - Magic: ");
    print_hex(magic);
    print(" Flags: ");
    print_hex(mbd->flags);
    print("\n");
    char* test_ptr = (char*)kmalloc(10);
    print("Test Alloc: ");
    print_hex((unsigned int)test_ptr);
    print("\n");
    if(magic!=MULTIBOOT_MAGIC){
        print("Invalid multiboot magic number \n");
        return;
    }

    if(!(mbd->flags & (1<<6))){
        print("Error in memory mapping (Bit 6 not set!)\n");
        return ;
    }

    print("MMap Addr: ");
    print_hex(mbd->mmap_addr);
    print(" Len: ");
    print_hex(mbd->mmap_length);
    print("\n");

    struct multiboot_mmap_entry* mmap =(struct multiboot_mmap_entry*)(mbd->mmap_addr);

    while((unsigned int)mmap < mbd->mmap_addr + mbd->mmap_length){
        if(mmap->type==1){
            total_memory += mmap->length_low;
        }
        mmap =(struct multiboot_mmap_entry*)((unsigned int)mmap + mmap->size+ sizeof(mmap->size));
    }
    
    print("Total Memory Calculated: ");
    print_hex(total_memory);
    print("\n");
}

void* kmalloc(unsigned int size){
    if(size% 4!=0){
        size+=4 - (size%4);
    }
    void* ptr =(void*)free_mem_addr;
    free_mem_addr+=size;
    allocated_memory+=size;
    return ptr;
}
void* kmalloc_a(unsigned int size) {
    // Check if free_mem_addr is NOT aligned to 4096 (0x1000)
    // 0xFFF is 4095. If the bottom 12 bits are not 0, it's not aligned!
    if (free_mem_addr & 0xFFF) {
        free_mem_addr &= 0xFFFFF000; // Clear the bottom 12 bits
        free_mem_addr += 0x1000;     // Bump it up to the next 4KB boundary
    }
    
    void* ptr = (void*)free_mem_addr;
    free_mem_addr += size;
    allocated_memory += size;
    
    return ptr;
}

// --- BITMAP HELPER FUNCTIONS ---
static void pmm_set_bit(uint32_t frame_idx) {
    mem_bitmap[frame_idx / 32] |= (1 << (frame_idx % 32));
}

static void pmm_clear_bit(uint32_t frame_idx) {
    mem_bitmap[frame_idx / 32] &= ~(1 << (frame_idx % 32));
}

static int pmm_test_bit(uint32_t frame_idx) {
    return mem_bitmap[frame_idx / 32] & (1 << (frame_idx % 32));
}

void init_pmm(struct multiboot_info* mbd) {
    // 1. Start with everything marked USED (all 1s).
    //    Any frame we don't explicitly know is safe RAM stays off-limits.
    for (int i = 0; i < 8192; i++) {
        mem_bitmap[i] = 0xFFFFFFFF;
    }

    // 2. Walk the GRUB memory map. For every region the BIOS says is usable
    //    (type == 1), mark those frames as FREE.
    //    This automatically keeps VGA buffer (0xA0000), BIOS ROM (0xE0000+),
    //    and any other reserved region as USED because we never clear them.
    if (mbd->flags & (1 << 6)) {
        struct multiboot_mmap_entry* mmap = (struct multiboot_mmap_entry*)mbd->mmap_addr;
        while ((uint32_t)mmap < mbd->mmap_addr + mbd->mmap_length) {
            if (mmap->type == 1 && mmap->base_addr_high == 0) {
                // Only handle regions in the first 4GB (base_addr_high == 0)
                uint32_t base  = mmap->base_addr_low;
                uint32_t len   = mmap->length_low;
                uint32_t first_frame = base / 4096;
                uint32_t num_frames  = len  / 4096;
                for (uint32_t i = 0; i < num_frames; i++) {
                    pmm_clear_bit(first_frame + i);
                }
                // Track total usable memory for max_frames
                if (first_frame + num_frames > max_frames)
                    max_frames = first_frame + num_frames;
            }
            mmap = (struct multiboot_mmap_entry*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
        }
    }

    // 3. Mark frame 0 (real-mode IVT + BDA) as used — never hand it out.
    pmm_set_bit(0);

    // 4. Mark everything the kernel and bump allocator have already consumed as USED.
    //    free_mem_addr is the next free byte from the bump allocator,
    //    so frames [0, ceil(free_mem_addr/4096)) are taken.
    uint32_t kernel_frames = (free_mem_addr + 4095) / 4096;
    for (uint32_t i = 0; i < kernel_frames; i++) {
        pmm_set_bit(i);
    }
}

uint32_t pmm_alloc_block() {
    // Look for the first 0 bit in the bitmap
    for (uint32_t i = 0; i < 8192; i++) {
        if (mem_bitmap[i] != 0xFFFFFFFF) { 
            for (int bit = 0; bit < 32; bit++) {
                if (!(mem_bitmap[i] & (1 << bit))) { // Found the 0 bit!
                    uint32_t frame = (i * 32) + bit;
                    if (frame >= max_frames) return 0; // Out of physical memory
                    
                    pmm_set_bit(frame); // Mark it as used
                    allocated_memory += 4096;
                    return frame * 4096; // Return the physical address
                }
            }
        }
    }
    return 0; 
}

void pmm_free_block(uint32_t p_addr) {
    uint32_t frame = p_addr / 4096;
    pmm_clear_bit(frame);
    allocated_memory -= 4096;
}

unsigned get_total_memory(){return total_memory;}
unsigned get_allocated_memory(){return allocated_memory;}
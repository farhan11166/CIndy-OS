#include "../include/memory.h"
#include "../include/screen.h"
unsigned int free_mem_addr= 0x200000;
unsigned int total_memory=0;

unsigned int allocated_memory=0;


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

unsigned get_total_memory(){return total_memory;}
unsigned get_allocated_memory(){return allocated_memory;}
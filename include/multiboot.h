#ifndef MULTIBOOT_H
#define MULTIBOOT_H
#define MULTIBOOT_MAGIC 0x2BADB002
struct multiboot_mmap_entry {
    unsigned int size;
    unsigned int base_addr_low;
    unsigned int base_addr_high;
    unsigned int length_low;
    unsigned int length_high;
    unsigned int type; // Type 1 = Usable RAM
} __attribute__((packed));

struct multiboot_info {
    unsigned int flags;
    unsigned int mem_lower;
    unsigned int mem_upper;
    unsigned int boot_device;
    unsigned int cmdline;
    unsigned int mods_count;
    unsigned int mods_addr;
    unsigned int num;
    unsigned int size;
    unsigned int addr;
    unsigned int shndx;
    unsigned int mmap_length;
    unsigned int mmap_addr;
} __attribute__((packed));
struct multiboot_mod_list {
    unsigned int mod_start;  // Physical memory address where the file begins
    unsigned int mod_end;    // Physical memory address where the file ends
    unsigned int cmdline;
    unsigned int pad;
} __attribute__((packed));
#endif
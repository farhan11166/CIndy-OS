#include "../include/fs.h"
#include "../include/screen.h"
#include "../include/string.h"

unsigned int initrd_address = 0;

void init_fs(unsigned int tar_address) {
    initrd_address = tar_address;
    print("File System loaded at: ");
    print_hex(initrd_address);
    print("\n");
}

// We will write these in the next step
void tar_ls() {}
void tar_cat(const char* filename) {}
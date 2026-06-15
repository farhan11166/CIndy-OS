#include "../include/fs.h"
#include "../include/screen.h"
#include "../include/string.h"

unsigned int initrd_address=0;
void init_fs(unsigned int tar_address){
    initrd_address = tar_address;
    print("File system loaded at: ");
    print_hex(initrd_address);
    print("\n");
}
unsigned int parse_octal(const char* str, int max_len) {
    unsigned int size = 0;
    for (int i = 0; i < max_len; i++) {
        if (str[i] >= '0' && str[i] <= '7') {
            size *= 8;
            size += (str[i] - '0');
        }
    }
    return size;
}

void tar_ls(){
    if(initrd_address==0) return ;

    unsigned int current_addr = initrd_address;
    while(1){
    struct tar_header *header = (struct tar_header*)current_addr;

     
    if(header->filename[0]=='\0'){ break;}

    unsigned int file_size = parse_octal(header->size,11);
    print(header->filename);
    print(" (");
    print_int(file_size);
    print(" bytes)  ");

    // Jump past the 512-byte header to the file data
    current_addr += 512;

    unsigned int padded_size=file_size;
     if (padded_size % 512) {
            padded_size += 512 - (padded_size % 512);
        }
        current_addr += padded_size;
    }
    print("\n");

    
    }
   
    

void tar_cat(const char* filename){
    if(initrd_address==0) return ;

    unsigned int current_addr = initrd_address;
    while(1){
     struct tar_header *header = (struct tar_header*)current_addr;

      if (header->filename[0] == '\0') {
            print("File not found.\n");
            return;
        }
        
        unsigned int file_size = parse_octal(header->size, 11);
        if (strcmp(header->filename, filename) == 0) {
            // The actual text is right after the 512-byte header
            char* file_data = (char*)(current_addr + 512);
            
            // Print out the file contents character by character
            for (unsigned int i = 0; i < file_size; i++) {
                char str[2] = {file_data[i], '\0'};
                print(str);
            }
            print("\n");
            return;
        }
          current_addr += 512;
        unsigned int padded_size = file_size;
        if (padded_size % 512 != 0) {
            padded_size += 512 - (padded_size % 512);
        }
        current_addr += padded_size;
       }
       
}
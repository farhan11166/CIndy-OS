#include "../include/ata.h"
#include "../include/types.h"
#include "../include/fat16.h"
#include "../include/screen.h"
 
void fat16_init(){
    uint8_t buffer[512];

    ata_read_sector(0,buffer);

    fat16_bpb_t* bpb =(fat16_bpb_t*) buffer;
     print("Bytes per sector: ");
    print_int(bpb->bytes_per_sector);
    print("\n");
    // 2. Verify FS Type safely
    // Create a temporary 9-byte array, copy the 8 bytes over, and add the null terminator.
    char fs_type_str[9];
    for (int i = 0; i < 8; i++) {
        fs_type_str[i] = bpb->fs_type[i];
    }
    fs_type_str[8] = '\0'; // Add the null terminator so print() knows when to stop!
    
    print("Filesystem Type: ");
    print(fs_type_str);
    print("\n");
    
    
    
}
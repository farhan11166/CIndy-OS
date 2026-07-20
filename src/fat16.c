#include "../include/ata.h"
#include "../include/types.h"
#include "../include/fat16.h"
#include "../include/screen.h"
 
void fat16_init() {
    uint8_t buffer[512];
    ata_read_sector(0, buffer);

    fat16_bpb_t* bpb = (fat16_bpb_t*)buffer;

    // Print basic BPB info
    print("Bytes per sector: ");
    print_int(bpb->bytes_per_sector);
    print("\n");

    char fs_type_str[9];
    for (int i = 0; i < 8; i++) {
        fs_type_str[i] = bpb->fs_type[i];
    }
    fs_type_str[8] = '\0';
    print("Filesystem Type: ");
    print(fs_type_str);
    print("\n");

    // Calculate where the root directory starts
    uint32_t root_dir_start = bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat);
    print("Root dir starts at sector: ");
    print_int(root_dir_start);
    print("\n");

    // Calculate how many sectors the root directory occupies
    // Each directory entry is 32 bytes, so entries_per_sector = 512 / 32 = 16
    uint32_t root_dir_sectors = (bpb->root_dir_entries * 32) / bpb->bytes_per_sector;

    print("Files on FAT16 disk:\n");
    print("--------------------\n");

    uint8_t sector_buf[512];
    int found_any = 0;

    // Loop through each sector of the root directory
    for (uint32_t s = 0; s < root_dir_sectors; s++) {
        ata_read_sector(root_dir_start + s, sector_buf);

        // Each sector has 16 directory entries (512 / 32 = 16)
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;

        for (int e = 0; e < 16; e++) {
            uint8_t first = entries[e].filename[0];

            // 0x00 = end of directory, nothing more follows
            if (first == 0x00) goto done;

            // 0xE5 = deleted file, skip it
            if (first == 0xE5) continue;

            // 0x0F = Long File Name (LFN) entry, skip it
            if (entries[e].attributes == 0x0F) continue;

            // Print the 8.3 filename
            found_any = 1;
            char name[9];
            char ext[4];

            for (int i = 0; i < 8; i++) name[i] = entries[e].filename[i];
            for (int i = 0; i < 3; i++) ext[i] = entries[e].ex[i];
            name[8] = '\0';
            ext[3] = '\0';

            print(name);
            print(".");
            print(ext);
            print("\n");
        }
    }

done:
    if (!found_any) {
        print("(no files found)\n");
    }
}
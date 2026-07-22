#include "../include/ata.h"
#include "../include/types.h"
#include "../include/fat16.h"
#include "../include/screen.h"
#include "../include/string.h"


void fat16_format_83(const char* input , char* name8, char* ext3){
    memset(name8,' ',8);
    memset(ext3,' ',3);
    int i=0;
    while(input[i] !='0' && input[i] !='.' && i<8){
        char c = input[i];
        if (c >= 'a' && c <= 'z') c -= 32; // Uppercase
        name8[i] = c;
        i++;

    }
    while (input[i] != '\0' && input[i] != '.') i++;
    if (input[i] == '.') {
        i++;
        int ext_i = 0;
        while (input[i] != '\0' && ext_i < 3) {
            char c = input[i];
            if (c >= 'a' && c <= 'z') c -= 32; // Uppercase
            ext3[ext_i] = c;
            i++;
            ext_i++;
        }
    }
}


static fat16_bpb_t   g_bpb;
static uint32_t      fat_start_sector  = 0;
static uint32_t      root_dir_start    = 0;
static uint32_t      root_dir_sectors  = 0;
static uint32_t      data_start_sector = 0;


static uint32_t fat16_cluster_to_lba(uint16_t cluster) {
    return data_start_sector + (uint32_t)(cluster - 2) * g_bpb.sectors_per_clustor;
}
static uint16_t fat16_read_fat_entry(uint16_t cluster) {
    uint32_t fat_offset   = cluster * 2;    // each FAT16 entry is 2 bytes
    uint32_t fat_sector   = fat_start_sector + (fat_offset / g_bpb.bytes_per_sector);
    uint32_t entry_offset = fat_offset % g_bpb.bytes_per_sector;
    uint8_t buf[512];
    ata_read_sector(fat_sector, buf);
    return *(uint16_t*)&buf[entry_offset];
}

static void fat16_write_fat_entry(uint16_t cluster,uint16_t value){
    uint32_t fat_offset= 2*cluster;
    uint32_t fat1_sector= fat_start_sector+ (fat_offset/g_bpb.bytes_per_sector);
    uint32_t entry_offset = fat_offset % g_bpb.bytes_per_sector;

    uint8_t buf[512];
    ata_read_sector(fat1_sector, buf);  
    *(uint16_t*)&buf[entry_offset] = value;
    ata_write_sector(fat1_sector, buf);

    uint32_t fat2_sector = fat1_sector + g_bpb.sectors_per_fat;
    ata_write_sector(fat2_sector, buf); 
}

static uint16_t fat16_find_free_cluster(){
    uint32_t total_sectors = (g_bpb.total_sectors_small!=0)
                              ? g_bpb.total_sectors_small
                              : g_bpb.total_sectors_large;

    uint32_t total_clusters = (total_sectors - data_start_sector) / g_bpb.sectors_per_clustor;
    for (uint16_t c = 2; c < (uint16_t)(total_clusters + 2); c++) {
        if (fat16_read_fat_entry(c) == 0x0000)
            return c;   // found a free one
    }
    
    return 0; // disk is fullll!!!!! </3

}





void fat16_init() {
    uint8_t buffer[512];
    ata_read_sector(0, buffer);

    fat16_bpb_t* bpb = (fat16_bpb_t*)buffer;
    g_bpb = *bpb;  // copy BPB data from stack buffer INTO the global struct (persists!)

   // Assign to GLOBALS (no 'uint32_t' keyword = no new local variable)
    fat_start_sector  = g_bpb.reserved_sectors;
    root_dir_start    = g_bpb.reserved_sectors + (g_bpb.fat_count * g_bpb.sectors_per_fat);
    root_dir_sectors  = (g_bpb.root_dir_entries * 32) / g_bpb.bytes_per_sector;
    data_start_sector = root_dir_start + root_dir_sectors;

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
   
    print("Root dir starts at sector: ");
    print_int(root_dir_start);
    print("\n");

    // Calculate how many sectors the root directory occupies
    // Each directory entry is 32 bytes, so entries_per_sector = 512 / 32 = 16
    

    fat16_ls();
}       // close fat16_init()

// Standalone file listing — callable from shell
void fat16_ls() {
    print("FAT16 Files:\n");
    print("------------\n");

    uint8_t sector_buf[512];   // local stack buffer — perfectly fine
    int found_any = 0;

    for (uint32_t s = 0; s < root_dir_sectors; s++) {
        ata_read_sector(root_dir_start + s, sector_buf);
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;

        for (int e = 0; e < 16; e++) {
            uint8_t first = entries[e].filename[0];
            if (first == 0x00) goto ls_done;
            if (first == 0xE5 || entries[e].attributes == 0x0F) continue;

            found_any = 1;
            char name[9]; char ext[4];
            for (int i = 0; i < 8; i++) name[i] = entries[e].filename[i];
            for (int i = 0; i < 3; i++) ext[i]  = entries[e].ex[i];
            name[8] = '\0'; ext[3] = '\0';

            print(name); print("."); print(ext);
            print("  (");
            print_int(entries[e].file_size);
            print(" bytes)\n");
        }
    }
ls_done:
    if (!found_any) print("(no files)\n");
}


int fat16_read_file(const char* name8, const char* ext3, uint8_t* out_buffer){
    fat16_dir_entry_t target;
    int found = 0;

    uint8_t sector_buf[512];

    for(uint32_t s=0; s<root_dir_sectors && !found; s++){
        ata_read_sector(root_dir_start + s, sector_buf);
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;


        for(int e=0;e<16;e++){
            if (entries[e].filename[0] == 0x00) goto not_found;
            if (entries[e].filename[0] == 0xE5) continue;
            if (entries[e].attributes == 0x0F)  continue;

            if (strncmp((char*)entries[e].filename, name8, 8) == 0 &&
                strncmp((char*)entries[e].ex,       ext3,  3) == 0) {
                target = entries[e];
                found  = 1;
                break;
            }



        }



    }

    not_found:
      if(!found){
        print("fat16_read: file not found\n");
        return -1;

      }

       uint16_t cluster    = target.low_first_cluster;
      uint32_t bytes_left = target.file_size;
       uint32_t out_pos    = 0;



    while (cluster >= 0x0002 && cluster < 0xFFF8 && bytes_left > 0) {
        uint32_t lba = fat16_cluster_to_lba(cluster);
        for (uint8_t sec = 0; sec < g_bpb.sectors_per_clustor && bytes_left > 0; sec++) {
            uint8_t tmp[512];
            ata_read_sector(lba + sec, tmp);
            uint32_t to_copy = (bytes_left < 512) ? bytes_left : 512;
            for (uint32_t i = 0; i < to_copy; i++)
                out_buffer[out_pos + i] = tmp[i];
            out_pos    += to_copy;
            bytes_left -= to_copy;
        }
        cluster = fat16_read_fat_entry(cluster);  // follow chain
    }  

   return (int)target.file_size;
}

int fat16_write_file(const char* name8, const char* ext3,const uint8_t* data,uint32_t size){
    uint32_t cluster_bytes   = g_bpb.sectors_per_clustor * g_bpb.bytes_per_sector;
    uint32_t clusters_needed = (size + cluster_bytes - 1) / cluster_bytes;
    if (clusters_needed == 0) {
        print("fat16_write: cannot write empty file\n");
        return -1;
    }

    uint16_t first_cluster = 0;
    uint16_t prev_cluster  = 0;


    for (uint32_t i = 0; i < clusters_needed; i++) {
        uint16_t free_c = fat16_find_free_cluster();
        if (free_c == 0) {
            print("fat16_write: disk full!\n");
            return -1;
        }
        if (i == 0) {
            first_cluster = free_c;         // remember the start
        } else {
            fat16_write_fat_entry(prev_cluster, free_c);  // link prev → this
        }
        fat16_write_fat_entry(free_c, 0xFFFF);  // mark as END for now
        prev_cluster = free_c;
    }
    uint16_t curr_cluster = first_cluster;
    uint32_t bytes_written = 0;

    while (curr_cluster >= 0x0002 && curr_cluster < 0xFFF8 && bytes_written < size) {
        uint32_t lba = fat16_cluster_to_lba(curr_cluster);
        for (uint8_t sec = 0; sec < g_bpb.sectors_per_clustor && bytes_written < size; sec++) {
            uint8_t tmp[512];
            memset(tmp, 0, 512);
            uint32_t chunk = (size - bytes_written < 512) ? (size - bytes_written) : 512;
            for (uint32_t i = 0; i < chunk; i++)
                tmp[i] = data[bytes_written + i];
            ata_write_sector(lba + sec, tmp);
            bytes_written += chunk;
        }
        curr_cluster = fat16_read_fat_entry(curr_cluster);
    }

    uint8_t sector_buf[512];
    for (uint32_t s = 0; s < root_dir_sectors; s++) {
        ata_read_sector(root_dir_start + s, sector_buf);
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buf;
        for (int e = 0; e < 16; e++) {
            if (entries[e].filename[0] == 0x00 || entries[e].filename[0] == 0xE5) {
                // Found empty slot — fill it
                memset(&entries[e], 0, 32);
                memcpy(entries[e].filename, name8, 8);
                memcpy(entries[e].ex,       ext3,  3);
                entries[e].attributes       = 0x20;          // archive
                entries[e].low_first_cluster = first_cluster;
                entries[e].file_size         = size;
                ata_write_sector(root_dir_start + s, sector_buf);
                print("fat16_write: file written!\n");
                return 0;
            }
        }
    }
    print("fat16_write: root directory full!\n");
    return -1;




}

#ifndef FAT16_H
#define FAT16_H

#include "types.h"

struct fat16_bpb{
    uint8_t jump[3];
    uint8_t oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_clustor;
    uint16_t reserved_sectors; //generallllyy the bpb
    uint8_t fat_count;
    uint16_t root_dir_entries;
    uint16_t total_sectors_small;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_large;
  // extended boot record for fat16 :-
    uint8_t drive_number;
    uint8_t reserved;
    uint8_t boot_signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t fs_type[8];
}__attribute__((packed));

typedef struct fat16_bpb fat16_bpb_t;

// rooot direc entries
struct fat16_dir_entry{
    uint8_t filename[8];
    uint8_t ex[3];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t high_first_cluster;    // Always 0 for FAT16
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint16_t low_first_cluster;     // The cluster where the file's data begins!
    uint32_t file_size;             // Size of file in bytes
}__attribute__((packed));

typedef struct fat16_dir_entry fat16_dir_entry_t;
void fat16_init();



#endif
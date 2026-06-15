#ifndef FS_H
#define FS_H
struct tar_header {
    char filename[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];       // The file size is stored as an ASCII Octal string!
    char mtime[12];
    char chksum[8];
    char typeflag[1];
    char padding[355];   // Pad the rest of the struct so it is exactly 512 bytes
} __attribute__((packed));

void init_fs(unsigned int tar_address);
void tar_ls();
void tar_cat(const char* filename);

unsigned int parse_octal(const char* str,int max_len);




#endif
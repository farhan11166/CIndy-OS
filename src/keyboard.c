#include "../include/keyboard.h"
#include "../include/screen.h"
#include "../include/ports.h"
#include "../include/memory.h"
#include "../include/fs.h"
#include "../include/timer.h"
#include "../include/string.h"
#include "../include/types.h"
#include "../include/ata.h"
#include "../include/fat16.h"  
#include "../include/paging.h" 
#include "../include/gdt.h"
extern volatile unsigned int timer_ticks;
extern uint8_t stack_top[];
extern uint32_t saved_kernel_esp;

struct elf32_ehdr {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct elf32_phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

static int load_elf_program(const uint8_t *file_buf, uint32_t file_size, uint32_t *entry_out) {
    if (file_size < 52) return -1;
    if (file_buf[0] != 0x7F || file_buf[1] != 'E' || file_buf[2] != 'L' || file_buf[3] != 'F') return -1;

    const struct elf32_ehdr *ehdr = (const struct elf32_ehdr *)file_buf;
    if (ehdr->e_type != 2) return -1; // ET_EXEC
    if (ehdr->e_phnum == 0) return -1;
    if (ehdr->e_phoff + (ehdr->e_phnum * ehdr->e_phentsize) > file_size) return -1;

    *entry_out = ehdr->e_entry;

    for (uint16_t i = 0; i < ehdr->e_phnum; ++i) {
        const struct elf32_phdr *ph = (const struct elf32_phdr *)(file_buf + ehdr->e_phoff + (i * ehdr->e_phentsize));
        if (ph->p_type != 1u) continue; // PT_LOAD
        if (ph->p_vaddr == 0) continue;

        uint32_t segment_pages = (ph->p_memsz + 4095u) / 4096u;
        for (uint32_t page = 0; page < segment_pages; ++page) {
            uint32_t vaddr = ph->p_vaddr + (page * 4096u);
            uint32_t phys = pmm_alloc_block();
            if (phys == 0) return -1;
            map_page(phys, vaddr, 7u);
        }

        uint32_t copy_len = ph->p_filesz;
        if (copy_len > ph->p_memsz) copy_len = ph->p_memsz;
        if (copy_len > 0) {
            uint8_t *dest = (uint8_t *)ph->p_vaddr;
            const uint8_t *src = file_buf + ph->p_offset;
            memcpy(dest, src, copy_len);
        }
        if (ph->p_memsz > ph->p_filesz) {
            memset((void *)(ph->p_vaddr + ph->p_filesz), 0, ph->p_memsz - ph->p_filesz);
        }
    }
    return 0;
}

uint8_t shift_pressed = 0;
uint8_t caps = 0;

const char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',   
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',   
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,   
  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' '
};
const char kbd_us_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0,
  '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0, '*',   0, ' '
};
#define BUFFER_SIZE 256
char
input_buffer[BUFFER_SIZE];
int buffer_index=0;

void execute_command(){
    print("\n");
    int argc=0;
    char *argv[16];
    for(int i=0;input_buffer[i]!='\0' && argc<16;){
        while(input_buffer[i]==' '){
            i++;
        }
        if(input_buffer[i]!='\0'){
            argv[argc]=&input_buffer[i];
            while(input_buffer[i]!='\0' && input_buffer[i]!=' '){
                i++;
            }
            if (input_buffer[i] == ' ') {//checking for space if it is eod the autmoatically the loop ends
            input_buffer[i] = '\0';
            i++;
            }
            argc++;
        }
    }
    if(argc>0){
        if(strcmp(argv[0],"help")==0){
            print("Available commands:\n");
            print("  help, clear, echo, timer, about, version, reboot\n");
            print("  meminfo, uptime, pwd, whoami\n");
            print("  ls, cat                  (initrd/tar filesystem)\n");
            print("  fat-ls                   (list FAT16 disk files)\n");
            print("  fat-cat  <file.ext>      (read  FAT16 file)\n");
            print("  fat-write <file.ext> <text>  (write FAT16 file)\n");
            print("  run <file.ext>           (load and execute binary)\n");
            print("  read-test, write-test    (raw ATA sector test)\n");

        }
        else if(strcmp(argv[0],"clear")==0){
            clear_screen();
        }
        else if (strcmp(argv[0], "timer") == 0) {
            print_int(timer_ticks / 100);
            print(" seconds");
        } 
        else if (strcmp(argv[0], "about") == 0) {
            print("CIndy-OS - A bare-metal hobby OS built from scratch.");
        } 
        else if (strcmp(argv[0], "version") == 0) {
            print("CIndy-OS Kernel v0.1");
        }
        else if (strcmp(argv[0], "reboot") == 0) {
            print("Rebooting...");
            outb(0x64, 0xFE); // Tell keyboard controller to pulse the CPU reset line
            while(1);         // Wait for the reset
        }
        else if(strcmp(argv[0],"echo")==0){
            if(argc==1){
                print("No string passed");
            }
            else{
                for(int i=1;i<argc;i++){
                    print(argv[i]);
                    if(i!=(argc-1)){
                        print(" ");
                    }
               }
            }
        }
        else if (strcmp(argv[0], "meminfo") == 0) {
            print("Total RAM: ");
            print_int(get_total_memory() / 1024);
            print(" KB\n");
            
            print("Allocated: ");
            print_int(get_allocated_memory());
            print(" Bytes\n");
        }
        else if (strcmp(argv[0], "ls") == 0) {
            tar_ls();
        }
        else if (strcmp(argv[0], "cat") == 0) {
            if (argc == 1) {
                print("Usage: cat <filename>\n");
            } else {
                tar_cat(argv[1]);
            }
        }
        else if (strcmp(argv[0], "pwd") == 0) {
       print("/");
        }
       else if (strcmp(argv[0], "whoami") == 0) {
         print("root");
        }
        else if (strcmp(argv[0], "uptime") == 0) {
        uint32_t seconds = timer_ticks / 100;
        uint32_t minutes = seconds / 60;
        uint32_t hours = minutes / 60;
        print_int(hours);
        print("h ");
        print_int(minutes % 60);
        print("m ");
        print_int(seconds % 60);
        print("s");
        }
        else if (strcmp(argv[0], "write-test") == 0) {
             if (argc < 2) {
                print("Usage: write_test <text>\n");
            } else {
                unsigned char buffer[512] = {0};
                
                // Copy all arguments into our buffer separated by spaces
                int buf_idx = 0;
                for (int a = 1; a < argc; a++) {
                    int c = 0;
                    while (argv[a][c] != '\0' && buf_idx < 510) {
                        buffer[buf_idx++] = argv[a][c++];
                    }
                    if (a < argc - 1 && buf_idx < 510) {
                        buffer[buf_idx++] = ' '; // Add space between words
                    }
                }
                buffer[buf_idx] = '\0'; // ensure it's null-terminated
                
                ata_write_sector(1, buffer);
                print("Successfully wrote to Sector 1!\n");
            }
           
        }
        else if (strcmp(argv[0], "read-test") == 0) {
            unsigned char buffer[512] = {0};
            ata_read_sector(1, buffer);
            print("Read from Sector 1: ");
            print((char*)buffer);
            print("\n");
        }
        else if (strcmp(argv[0], "fat-ls") == 0) {
            fat16_ls();
        }
        else if (strcmp(argv[0], "fat-cat") == 0) {
            if (argc < 2) {
                print("Usage: fat-cat <filename.ext>\n");
            } else {
                char name8[9], ext3[4];
                fat16_format_83(argv[1], name8, ext3);
                uint8_t buf[4096];
                int bytes = fat16_read_file(name8, ext3, buf, sizeof(buf));
                if (bytes > 0) {
                    buf[bytes] = '\0';
                    print((char*)buf);
                    print("\n");
                }
            }
        }
        else if (strcmp(argv[0], "fat-write") == 0) {
            if (argc < 3) {
                print("Usage: fat-write <filename.ext> <text>\n");
            } else {
                char name8[9], ext3[4];
                fat16_format_83(argv[1], name8, ext3);

                // Combine all remaining args as the file content
                char content[256];
                int idx = 0;
                for (int a = 2; a < argc && idx < 254; a++) {
                    int c = 0;
                    while (argv[a][c] != '\0' && idx < 254)
                        content[idx++] = argv[a][c++];
                    if (a < argc - 1 && idx < 254)
                        content[idx++] = ' ';
                }
                content[idx] = '\0';

                fat16_write_file(name8, ext3, (const uint8_t*)content, idx);
            }
        }
        else if(!strcmp(argv[0],"run")){
            if (argc < 2) {
                print("Usage: run <filename.ext>\n");
            } else {
                uint32_t phy_block = pmm_alloc_block();
                if (phy_block == 0) {
                    print("run: out of physical memory\n");
                } else {
                    map_page(phy_block, 0x40000000, 7); // present + R/W + user
                    char name8[9], ext3[4];
                    fat16_format_83(argv[1], name8, ext3);

                    int result = fat16_read_file(name8, ext3, (uint8_t*)0x40000000, 4096);
                    if (result < 0) {
                        print("run: failed to load file\n");
                    } else {
                        uint32_t user_entry = 0x40000000;
                        const uint8_t *img = (const uint8_t *)0x40000000;
                        if (img[0] == 0x7F && img[1] == 'E' && img[2] == 'L' && img[3] == 'F') {
                            if (load_elf_program(img, (uint32_t)result, &user_entry) < 0) {
                                print("run: invalid ELF image\n");
                                return;
                            }
                        }

                        uint32_t user_stack_phys = pmm_alloc_block();
                        map_page(user_stack_phys, 0x80000000, 7); // present + R/W + user
                        uint32_t user_stack_top = 0x80000FFC; // within the mapped page

                        print("Loaded ");
                        print_int(result);
                        print(" bytes, jumping to ring 3 at 0x");
                        print_hex(user_entry);
                        print("...\n");
                        tss_set_kernel_stack((uint32_t)&stack_top);
                        debug_print("about to call jump_to_usermode\n");
                        jump_to_usermode(user_stack_top, user_entry);
                        debug_print("jump_to_usermode returned\n");
                        /* kernel_return() in usermode.asm resumes here on exit syscall */
                        print("Program exited.\n");
                    }
                }
            }
        }
        else{
            print("Unknown command: ");
            print(argv[0]);
        }
    }
    print("\n");
    print_colored("[CIndy-OS]> ", 0x0B);

    buffer_index=0;
    for(int i=0;i<BUFFER_SIZE;i++){
        input_buffer[i]=0;
    }
}

void handle_keyboard_interrupt(unsigned char scancode) {
    switch(scancode) {
        case 0x2A: // Left Shift pressed
        case 0x36: // Right Shift pressed
            shift_pressed = 1;
            return;
        case 0xAA: // Left Shift released
        case 0xB6: // Right Shift released
            shift_pressed = 0;
            return;
        case 0x3A: // Caps Lock
            caps = (caps + 1) % 2;
            return;
    }

    if (scancode & 0x80) { // ignore other release events
        return;
    }
    
    char ascii = shift_pressed ? kbd_us_shift[scancode] : kbd_us[scancode];
    
    if (ascii == '\b') {
        if (buffer_index > 0) {
            buffer_index--;
            input_buffer[buffer_index] = '\0';
            print("\b \b");
        }
    }
    else if (ascii == '\n') {
        input_buffer[buffer_index] = '\0';
        execute_command();
    }
    else if (ascii != 0) {
        if (ascii >= 'a' && ascii <= 'z') {
            if (caps) ascii -= 32; // a -> A
        }
        else if (ascii >= 'A' && ascii <= 'Z') {
            if(caps) ascii +=32;
        }

        if (buffer_index < BUFFER_SIZE - 1) {
            input_buffer[buffer_index] = ascii;
            char str[2] = {ascii, '\0'};
            print(str);
            buffer_index++;
        }
    }
}


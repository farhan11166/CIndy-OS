#include "../include/keyboard.h"
#include "../include/screen.h"
#include "../include/ports.h"
#include "../include/memory.h"
#include "../include/fs.h"
#include "../include/timer.h"
#include "../include/string.h"
extern volatile unsigned int timer_ticks;
const char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',   
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',   
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,   
  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' '
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
            print("Available commands: help, clear, echo, timer, about, version, reboot,meminfo,ls,cat");
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

void handle_keyboard_interrupt(unsigned char scancode){
    if (scancode & 0x80) {
        return;
    }
    char ascii=kbd_us[scancode];
    if(ascii=='\b'){
        if(buffer_index>0){
            buffer_index--;
            input_buffer[buffer_index]='\0';
            print("\b \b");
            
        }}
        else if(ascii=='\n'){
            input_buffer[buffer_index]='\0';
            execute_command();
        }
        else if(ascii!=0){
            if(buffer_index<BUFFER_SIZE-1){
                input_buffer[buffer_index]=ascii;

                char str[2]={ascii,'\0'};
                print(str);
                buffer_index++;
            }
        }
        
    }


#include "../include/keyboard.h"
#include "../include/screen.h"
#include "../include/ports.h"
#include "../include/timer.h"
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
    if (input_buffer[0] == 'h' && input_buffer[1] == 'e' && input_buffer[2] == 'l' && input_buffer[3] == 'p' && buffer_index==4) {
        print("Available commands: help, clear,echo,timer");
        
    } else if (input_buffer[0] == 'c' && input_buffer[1] == 'l' && input_buffer[2] == 'e' && input_buffer[3] == 'a' && input_buffer[4] == 'r'&& buffer_index==5) {
        clear_screen();
    } else if (input_buffer[0] == 't' && input_buffer[1] == 'i' && input_buffer[2] == 'm' && input_buffer[3] == 'e' && input_buffer[4] == 'r'&& buffer_index==5) {
        print_int(timer_ticks/100);
        print(" seconds");
    }
    else if(input_buffer[0]=='e' && input_buffer[1]=='c' && input_buffer[2]=='h' && input_buffer[3]=='o' && buffer_index>=4){
        if(buffer_index==4){
            print("No string passed \n");
        }
        else{
            if(input_buffer[5]!='\0'){
                print(input_buffer+5);
            }
            print("\n");
    }
    }
     else if (buffer_index > 0) {
        print("Unknown command: ");
        print(input_buffer);
    }
    
    print("\n[CIndy-OS]> ");

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


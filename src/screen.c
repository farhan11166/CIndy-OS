#include "../include/screen.h"

volatile char* video_memory =(volatile char*) 0xb8000;

int cursor_row=0;
int cursor_col=0;

static void debug_putc(char c) {
    __asm__ volatile("outb %0, $0xE9" : : "a"(c));
}

void clear_screen(){
    for(int i=0;i<80*25;i++){
        video_memory[i*2]=' ';
        video_memory[i*2 + 1 ]=0x0F;}
        cursor_row=0;
        cursor_col=0;
}

void print_at(const char* msg , int row , int col){
    int offset= (80*row + col)*2;

    for(int i=0;msg[i]!='\0';i++){
        video_memory[offset+ i*2]=msg[i];
        video_memory[offset + i*2 +1]=0x6F;
    }


}
void print(const char* msg) {
    int offset = (cursor_row * 80 + cursor_col) * 2;

    for (int i = 0; msg[i] != '\0'; i++) {

        if (msg[i] == '\n') {
            debug_putc('\n');
            cursor_row++;
            cursor_col = 0;
            offset = (cursor_row * 80 + cursor_col) * 2;
            continue;
        }
        if(msg[i]=='\b'){
        if(cursor_col>0){
            cursor_col--;
        }
        else if(cursor_row>0){
            cursor_row--;
            cursor_col=79;
        }
        offset=(cursor_row*80 + cursor_col)*2;

        continue;
        }

        video_memory[offset] = msg[i];
        video_memory[offset + 1] = 0x6F;
        debug_putc(msg[i]);

        offset += 2;
        cursor_col++;

        if (cursor_col >= 80) {
            cursor_col = 0;
            cursor_row++;
        }
    }
}
void print_int(int num){
    char buffer [16];
    int i=0;
    if(num==0){
        print("0");
        return;
    }
    if(num<0){
        print("-");
        num=-num;
    }
    while (num>0){
        buffer[i++] = (num%10)+'0';
        num/=10;
    }
    for(int j=i-1;j>=0;j--){
        char str[2]; 
        str[0] = buffer[j]; str[1] = '\0'; 
        print(str);
    }
}

void print_hex(unsigned int num){
    char* hex_chars = "0123456789ABCDEF";
    print("0X");
     for (int i = 28; i >= 0; i -= 4) {

        unsigned int digit = (num >> i) & 0xF;

        char str[2];
        str[0] = hex_chars[digit];
        str[1] = '\0';

        print(str);
    }
    
}

#include "../include/screen.h"
#include "../include/ports.h"

volatile char* video_memory =(volatile char*) 0xb8000;

int cursor_row=0;
int cursor_col=0;
#define MAX_ROWS 25
#define MAX_COLS 80

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
        }
        else if (msg[i] == '\b') {
            if (cursor_col > 0) {
                cursor_col--;
            } else if (cursor_row > 0) {
                cursor_row--;
                cursor_col = 79;
            }
        }
        else {
            int offset = (cursor_row * 80 + cursor_col) * 2;
            video_memory[offset] = msg[i];
            video_memory[offset + 1] = 0x6F; // brown!
            debug_putc(msg[i]);

            cursor_col++;
            if (cursor_col >= 80) {
                cursor_col = 0;
                cursor_row++;
            }
        }

        // Check for scrolling AFTER updating cursor positions
        if (cursor_row >= MAX_ROWS) {
            scroll_screen();
            // scroll_screen already sets cursor_row to MAX_ROWS - 1
            // We do NOT touch cursor_col, because we want to continue printing on the same line if it just wrapped!
        }
    }
    update_cursor();
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
void scroll_screen(){
    // Copy rows 1-24 UP to rows 0-23
    for(int row=1;row<MAX_ROWS;row++){
        for(int col=0;col<MAX_COLS;col++){
            video_memory[((row-1)*80+col)*2] = video_memory[(row*80+col)*2];
            video_memory[((row-1)*80+col)*2+1] = video_memory[(row*80+col)*2+1];
        }
    }

    //clear last row
    for(int col=0;col<MAX_COLS;col++){
        int offset = ((MAX_ROWS - 1) * MAX_COLS + col) * 2;
        video_memory[offset] = ' ';
        video_memory[offset + 1] = 0x0F;
    }
    cursor_row=MAX_ROWS-1;
    
}
void print_colored(const char* message , unsigned char color){
    int i=0;
    while(message[i]!='\0'){
        if(message[i]=='\n'){
            cursor_row++;
            cursor_col=0;
        }
        else{
            int offset=(cursor_row*80+cursor_col)*2;
            video_memory[offset]=message[i];
            video_memory[offset+1]=color;
            cursor_col++;
            if (cursor_col >= MAX_COLS) {
                cursor_col = 0;
                cursor_row++;
            }
        }
         if (cursor_row >= MAX_ROWS) {
            scroll_screen();
        }
        i++;
    }
    update_cursor();
}
void update_cursor() {
    unsigned short pos = cursor_row * MAX_COLS + cursor_col;
    
    // VGA cursor command ports are 0x3D4 and 0x3D5
    // 0x0F = low byte, 0x0E = high byte
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
}

void enable_cursor(unsigned char cursor_start, unsigned char cursor_end) {
    // 0x0A is the cursor start register
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);
 
    // 0x0B is the cursor end register
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

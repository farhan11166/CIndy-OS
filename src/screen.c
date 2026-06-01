#include "../include/screen.h"

volatile char* video_memory =(volatile char*) 0xb8000;

int cursor_row=0;
int cursor_col=0;

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
            cursor_row++;
            cursor_col = 0;
            offset = (cursor_row * 80 + cursor_col) * 2;
            continue;
        }

        video_memory[offset] = msg[i];
        video_memory[offset + 1] = 0x6F;

        offset += 2;
        cursor_col++;

        if (cursor_col >= 80) {
            cursor_col = 0;
            cursor_row++;
        }
    }
}
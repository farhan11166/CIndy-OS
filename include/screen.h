#ifndef SCREEN_H
#define SCREEN_H

void clear_screen();
void print(const char* str);
void print_at(const char* str, int row, int col);
void print_int(int num);
void print_hex(unsigned int num);
void scroll_screen();
void print_colored(const char* message, unsigned char color);
void update_cursor();
void enable_cursor(unsigned char cursor_start, unsigned char cursor_end);


#endif
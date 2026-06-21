#include "../include/ports.h"

unsigned char inb(unsigned short port){
    unsigned char res;
    __asm__ volatile(
        "in %%dx ,%%al"
        : "=a" (res)
        : "d" (port)
    );
    return res;
}

void outb(unsigned short port, unsigned char data) {

    __asm__ volatile (
        "out %%al, %%dx"
        :
        : "a" (data), "d" (port)
    );
}

unsigned short inw(unsigned short port) {
    unsigned short res;
    __asm__ volatile (
        "inw %%dx, %%ax"
        : "=a" (res)
        : "Nd" (port)
    );
    return res;
}
void outw(unsigned short port, unsigned short data) {
    __asm__ volatile (
        "outw %%ax, %%dx"
        :
        : "a" (data), "Nd" (port)
    );
}
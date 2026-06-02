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
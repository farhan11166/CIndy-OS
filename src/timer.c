#include "../include/timer.h"
#include "../include/ports.h"
#include "../include/screen.h"

volatile unsigned int timer_ticks=0;

void timer_handler(){
    timer_ticks++;

    outb(0x20,0x20);

}
void init_timer(unsigned int freq){
    unsigned int divisor =1193180/freq;
    outb(0x43,0x36);
    unsigned char low=(unsigned char)(divisor & 0xFF);
    unsigned char high=(unsigned char)((divisor>>8)& 0xFF);
    outb(0x40,low);
    outb(0x40,high);

}
#include "../include/ports.h"
#include "../include/pic.h"

void pic_remap() {

    // starts initialization sequence
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    // remap offsets
    outb(0x21, 0x20);
    outb(0xA1, 0x28);

    // setup cascading
    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    // environment info
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    // unmask interrupts
    outb(0x21, 0x00);
    outb(0xA1, 0x00);
}

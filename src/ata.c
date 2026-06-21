#include "../include/ata.h"
#include "../include/ports.h"
#include "../include/types.h"
void ata_wait_ready(){
    while (inb(0x1F7) & 0x80){
        asm volatile("pause");
   }
   while(!(inb(0x1F7) & 0x40)){}
    




}
void ata_read_sector(unsigned int lba, unsigned char* buffer){
    ata_wait_ready();
      outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)(lba));        // bits 0-7
 outb(0x1F4, (uint8_t)(lba >> 8));   // bits 8-15
 outb(0x1F5, (uint8_t)(lba >> 16));  // bits 16-23
  outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F)); // bits 24-27

  outb(0x1F7,0X20);
  inb(0x3F6);
  inb(0x3F6);
  inb(0x3F6);
  inb(0x3F6);
  while (inb(0x1F7) & 0x80);      // BSY
  while (!(inb(0x1F7) & 0x08));   // DRQ
  unsigned short *ptr = (unsigned short *) buffer;
  for (int i = 0; i < 256; i++) {
        ptr[i] = inw(0x1F0);
    }
    inb(0x1F7);

}
void ata_write_sector(unsigned int lba, unsigned char* buffer){
    ata_wait_ready();
      outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)(lba));        // bits 0-7
 outb(0x1F4, (uint8_t)(lba >> 8));   // bits 8-15
 outb(0x1F5, (uint8_t)(lba >> 16));  // bits 16-23
  outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F)); // bits 24-27

  outb(0x1F7,0X30);
  inb(0x3F6);
  inb(0x3F6);
  inb(0x3F6);
  inb(0x3F6);
  while (inb(0x1F7) & 0x80);      // BSY
  while (!(inb(0x1F7) & 0x08));   // DRQ
  unsigned short *ptr = (unsigned short *) buffer;
  for (int i = 0; i < 256; i++) {
        outw(0x1F0,ptr[i]);
    }
    outb(0x1F7, 0xE7);
    inb(0x1F7);

}
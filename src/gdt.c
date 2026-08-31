#include "../include/gdt.h"
#include "../include/types.h"

//esp stack pointer
//ss stack segment


struct tss_entry{
    uint32_t prev_tss;   // The previous TSS - if we used hardware task switching this would form a linked list.
    uint32_t esp0; //kernel stack pointer loaded on ring3->ring0
    uint32_t ss0;  //kernel stack segment;
    uint32_t esp1; //ring1 stack pointer
    uint32_t ss1; // ring1 stack segement
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

static struct tss_entry tss;

extern uint8_t gdt_tss[16];
/* */
extern void tss_flush(uint16_t selector);


static void memset_tss(void){
    uint8_t *p =(uint8_t *)&tss;
    for(int i=0;i<sizeof(tss);i++){
        p[i]=0;
    }

}

 void gdt_init_tss(uint32_t kernel_stack_top) {
      memset_tss();

      tss.ss0        = KERNEL_DATA_SEG;
      tss.esp0       = kernel_stack_top;
     tss.cs         = KERNEL_CODE_SEG;
     tss.ds         = KERNEL_DATA_SEG;
     tss.es         = KERNEL_DATA_SEG;
     tss.fs         = KERNEL_DATA_SEG;
     tss.gs         = KERNEL_DATA_SEG;
     tss.ss         = KERNEL_DATA_SEG;
     tss.iomap_base = sizeof(tss); /* no I/O permission bitmap */

     write_tss_descriptor((uint32_t)&tss, sizeof(tss) - 1);
     tss_flush(TSS_SEG);
 }

 void tss_set_kernel_stack(uint32_t kernel_stack_top) {
      tss.esp0 = kernel_stack_top;
  }
void write_tss_descriptor(uint32_t base, uint32_t limit){
    uint8_t *entry = gdt_tss;

    /* 32-bit TSS descriptor is 16 bytes wide.
     * bytes 0..7: limit/base + type/flags
     * bytes 8..15: upper base bits + reserved
     */
    for (int i = 0; i < 16; i++) {
        entry[i] = 0;
    }

    entry[0] = limit & 0xFF;
    entry[1] = (limit >> 8) & 0xFF;
    entry[2] = base & 0xFF;
    entry[3] = (base >> 8) & 0xFF;
    entry[4] = (base >> 16) & 0xFF;
    entry[5] = 0x89;
    entry[6] = ((limit >> 16) & 0x0F) | 0x40;
    entry[7] = (base >> 24) & 0xFF;

    /* upper 32 bits of base are zero in this kernel setup */
}







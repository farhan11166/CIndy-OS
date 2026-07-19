#include "../include/paging.h"
#include "../include/memory.h"
#include "../include/types.h"



uint32_t* page_direc;
uint32_t* first_page_table;
void init_paging(){
page_direc= (uint32_t*)kmalloc_a(1024*4);
first_page_table=(uint32_t*)kmalloc_a(1024*4);
for(int i=0;i<1024;i++){
    //here we map each of the first tbale entry to 4kb space with present and read permission
     first_page_table[i] = (i * 0x1000) | 3;
}
// first one is for kernel only
page_direc[0]=((uint32_t)first_page_table|3);

for(int i=1;i<1024;i++){
    page_direc[i]=0;
    // to state nothing is present
}

  //loads physical adddr of page_direc tinto cr3 reg.
 asm volatile("mov %0, %%cr3":: "r"(page_direc));
 uint32_t cr0;
  // our int cr0 gets from cr0 then we set 31st bit to 1 so to enable the paging from cpu side;
  asm volatile("mov %%cr0, %0": "=r"(cr0));

  cr0 |= 0x80000000;

   asm volatile("mov %0, %%cr0":: "r"(cr0));





}
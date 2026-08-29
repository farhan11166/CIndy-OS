#ifndef REGS_H
#define REGS_H

#include <stdint.h>

/* Order matches pushad/pusha: eax, ecx, edx, ebx, esp, ebp, esi, edi */
struct regs {
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
};

#endif

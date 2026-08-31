#ifndef REGS_H
#define REGS_H

#include <stdint.h>

/* Memory order after `pusha`: EDI is at the lowest address (current ESP). */
struct regs {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
};

#endif

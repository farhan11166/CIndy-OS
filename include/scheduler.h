#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "regs.h"
#include "process.h"

void schedule(void);
void yield_kernel(void);

#endif

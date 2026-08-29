#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "regs.h"

#define SYS_EXIT   0
#define SYS_WRITE  1
#define SYS_OPEN   2
#define SYS_CLOSE  3
#define SYS_READ   4

#define FD_STDIN  0
#define FD_STDOUT 1
#define FD_STDERR 2

/* implement this in src/syscall.c */
void handle_syscall(struct regs *r);

#endif

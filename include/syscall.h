#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "regs.h"

#define SYS_EXIT   0
#define SYS_WRITE  1
#define SYS_OPEN   2
#define SYS_CLOSE  3
#define SYS_READ   4
#define SYS_YIELD  5

#define FD_STDIN  0
#define FD_STDOUT 1
#define FD_STDERR 2

#define USER_SPACE_START 0x40000000U
#define USER_SPACE_END   0xBFFFFFFFU

/* User-memory validation helpers used by syscall handlers. */
int validate_user_ptr(const void *ptr, uint32_t len);
int copy_from_user(void *dst, const void *user_src, uint32_t len);
int copy_to_user(void *user_dst, const void *src, uint32_t len);

/* implement this in src/syscall.c */
void handle_syscall(struct regs *r);

#endif

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "regs.h"

#define PROC_MAX_FDS 8

typedef enum{
    FD_TYPE_NONE=0,
    FD_TYPE_STUDIO, // 
    FD_TYPE_FAT16
} fd_type_t;

struct fd_entry {
      fd_type_t type;
      char      name8[9];    /* FAT 8.3 name */
      char      ext3[4];     /* FAT 8.3 ext  */
      uint32_t  offset;      /* read cursor  */
};

enum proc_state{
    PROC_NEW=0,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_EXITED
};

struct process{
    uint32_t pid;
    enum proc_state state;
    
    uint32_t entry;
    uint32_t esp;
    uint32_t user_stack_top;

    struct regs regs;
    uint32_t saved_eip; /* saved user EIP for returning from interrupts/syscalls */

    void *kernel_stack;
    void *page_dir;

    struct process *next;

    struct fd_entry fds[PROC_MAX_FDS];   
};

void process_init(void);
int task_create_from_elf(const char *path, uint32_t *pid_out);
struct process *proc_next_ready(void);
struct process *get_curr_proc(void);
struct process *get_proc_by_pid(uint32_t pid);
void proc_enqueue(struct process *proc);
struct process *proc_dequeue(void);
void proc_yield(void);
#endif
#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "regs.h"

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

    void *kernel_stack;
    void *page_dir;

    struct process *next;
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
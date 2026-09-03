#include "../include/process.h"
#include "../include/screen.h"
#include "../include/memory.h"
#include "../include/paging.h"
#include "../include/fs.h"
#include "../include/string.h"
#define NULL 0
static struct process *curr_proc = NULL;
static struct process *ready_head = NULL;
static struct process *ready_tail = NULL;
static uint32_t next_pid = 1;

static void queue_push(struct process *p) {
    if (!p) return;

    p->next = NULL;
    if (!ready_head) {
        ready_head = p;
        ready_tail = p;
        return;
    }

    ready_tail->next = p;
    ready_tail = p;
}

static struct process *queue_pop(void) {
    struct process *p = ready_head;
    if (!p) return NULL;

    ready_head = p->next;
    if (!ready_head) ready_tail = NULL;

    p->next = NULL;
    return p;
}

void process_init(void) {
    curr_proc = NULL;
    ready_head = NULL;
    ready_tail = NULL;
    next_pid = 1;
}

struct process *get_curr_proc(void) {
    return curr_proc;
}

struct process *proc_next_ready(void) {
    return ready_head;
}

struct process *get_proc_by_pid(uint32_t pid) {
    struct process *p = ready_head;
    while (p) {
        if (p->pid == pid) return p;
        p = p->next;
    }

    if (curr_proc && curr_proc->pid == pid) return curr_proc;
    return NULL;
}

void proc_enqueue(struct process *p) {
    if (!p) return;
    p->state = PROC_READY;
    queue_push(p);
}

struct process *proc_dequeue(void) {
    struct process *next = queue_pop();
    if (!next) return NULL;

    next->state = PROC_RUNNING;
    curr_proc = next;
    return next;
}

void proc_yield(void) {
    if (!curr_proc) return;

    curr_proc->state = PROC_READY;
    queue_push(curr_proc);

    struct process *next = queue_pop();
    if (!next) {
        curr_proc = NULL;
        return;
    }

    next->state = PROC_RUNNING;
    curr_proc = next;
}

int task_create_from_elf(const char *path, uint32_t *pid_out) {
    (void)path;
    (void)pid_out;
    return -1;
}
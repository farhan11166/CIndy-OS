#include "../include/process.h"
#include "../include/screen.h"
#include "../include/memory.h"
#include "../include/paging.h"
#include "../include/fs.h"
#include "../include/string.h"
#include "../include/fat16.h"
#include "../include/keyboard.h"
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
    if (!next) return 0;

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
    if(!path) return -1;

    char name8[9],ext3[4];
    fat16_format_83(path,name8,ext3);
    uint8_t *buf= (uint8_t*)kmalloc_a(4096);// allocation of 4kb buffer to read the elf file
    if(!buf) return -1;

    int bytes = fat16_read_file(name8,ext3,buf,4096); // read the elf file into the buffer and return the space taken
    if(bytes<0) return -1;

    if(bytes<4 || buf[0]!=0x7F || buf[1]!='E' || buf[2]!='L' || buf[3]!='F') return -1;

    uint32_t entry=0;
    if(load_elf_program(buf,(uint32_t)bytes,&entry)<0) return -1;

    uint32_t stack_phys=pmm_alloc_block();
    if(stack_phys==0) return -1;
    map_page(stack_phys,0x80000000u,7u); 
    memset((void*)0x80000000u,0,4096u);

    struct process *p=(struct process*)kmalloc_a(sizeof(struct process));

    if(!p) return -1;

    uint8_t *pb=(uint8_t *)p;

    for(uint32_t i=0;i<sizeof(struct process);i++) {
        pb[i] = 0;
    }
    p->pid           = next_pid++;
    p->state         = PROC_NEW;
    p->entry         = entry;
    p->esp           = 0x80000FFCu;
    p->user_stack_top = 0x80000FFCu;
    p->kernel_stack  = NULL;
    p->page_dir      = NULL;
    p->next          = NULL;

    if(pid_out) *pid_out=p->pid;
    proc_enqueue(p);

    print("[process] created pid=");
    print_int(p->pid);
    print(" entry=0x");
    print_hex(p->entry);
    print(" state=READY\n");

    return 0;


}
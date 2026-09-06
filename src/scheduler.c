#include "../include/scheduler.h"
#include "../include/syscall.h"
#include "../include/screen.h"
#include "../include/process.h"


void schedule(void){
    struct process *curr =get_curr_proc();
    if(curr && curr->state==PROC_RUNNING){
        proc_enqueue(curr);
    }


struct process *next = proc_dequeue();
if(!next){
    return;
}

print("[sched] switching to pid=");
print_int(next->pid);
print(" entry=0x");
print_hex(next->entry);
print("\n");

extern void jump_to_usermode(uint32_t user_stack, uint32_t user_entry);
      jump_to_usermode(next->user_stack_top ? next->user_stack_top : next->esp,
                       next->entry);

}                       

void yeild_kernel(void){
    schedule();
}

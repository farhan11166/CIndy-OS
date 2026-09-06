#include "../include/syscall.h"
#include "../include/screen.h"
#include "../include/paging.h"
#include "../include/process.h"

extern volatile uint32_t syscall_exit_requested;
volatile uint32_t last_exit_status = 0;
extern volatile uint32_t syscall_kernel_fallback;



int validate_user_ptr(const void *ptr, uint32_t len) {
    if (!ptr) return 0;
    if(syscall_kernel_fallback) return 1;
    uint32_t p = (uint32_t)ptr;
    return validate_user_region(p, len);
}

int copy_from_user(void *dst, const void *user_src, uint32_t len) {
    if(!dst || !user_src) return -1;
    if (!validate_user_ptr(user_src, len)) return -1;
    char *d = (char *)dst;
    const char *s = (const char *)user_src;
    for (uint32_t i = 0; i < len; ++i) d[i] = s[i];
    return 0;
}

int copy_to_user(void *user_dst, const void *src, uint32_t len) {
    if(!user_dst || !src) return -1;
    if (!validate_user_ptr(user_dst, len)) return -1;
    char *d = (char *)user_dst;
    const char *s = (const char *)src;
    for (uint32_t i = 0; i < len; ++i) d[i] = s[i];
    return 0;
}


void handle_syscall(struct regs *r) {
    uint32_t num = r->eax;
    uint32_t a = r->ebx;
    uint32_t b = r->ecx;
    uint32_t c = r->edx;
    // agent_dbg("C", "syscall.c:handle_syscall", "regs_as_struct", num, a, b, c);
    // agent_dbg("C", "syscall.c:handle_syscall", "pusha_edi_then_eax", raw[0], raw[3], raw[4], raw[7]);

    /* If the syscall was invoked from the kernel-mode ELF fallback, bypass
     * user-pointer validation. This is a temporary shim to let early testing
     * exercise syscalls while ring-3 transition is still in progress. */
    int kernel_mode_call = (syscall_kernel_fallback != 0);

    switch (num) {
    case SYS_EXIT:
        last_exit_status = a;
        syscall_exit_requested = 1;
        r->eax = 0;
        break;

    case SYS_WRITE: {
        int fd = (int)a;
        const char *user_buf = (const char *)b;
        uint32_t len = c;
        if (fd == FD_STDOUT || fd == FD_STDERR) {
            const uint32_t CHUNK = 1024;
            char tmp[CHUNK + 1];
            uint32_t offset = 0;
            uint32_t written = 0;
            while (offset < len) {
                uint32_t take = (len - offset) > CHUNK ? CHUNK : (len - offset);
                if (kernel_mode_call) {
                    /* Directly copy when caller is kernel-mode (testing fallback) */
                    for (uint32_t i = 0; i < take; ++i) tmp[i] = *(user_buf + offset + i);
                } else {
                    if (copy_from_user(tmp, user_buf + offset, take) < 0) break;
                }
                tmp[take] = '\0';
                print(tmp);
                offset += take;
                written += take;
            }
            r->eax = written;
            break;
        }
        struct process *proc =get_curr_proc();
        if(proc && fd>=0 && fd<PROC_MAX_FDS){
            struct fd_entry *fde=&proc->fds[fd];
            if(fde->type == FD_TYPE_FAT16){
                r->eax=-1;
                break;
            }
        }
        r->eax = -1;
        break;
    }

    case SYS_READ: {
        int fd = (int)a;
        char *user_buf = (char *)b;
        uint32_t len = c;
        if (fd == FD_STDIN) {
            /* stdin is not yet backed by a real keyboard buffer; return 0 */
            r->eax = 0;
            break;
        }
        r->eax = -1;
        break;
    }

    case SYS_OPEN: {
        const char *path = (const char *)a;
        uint32_t flags = b;
        (void)flags;
        if (!path || (!kernel_mode_call && !validate_user_ptr(path, 1))) {
            r->eax = -1;
            break;
        }
        /* For now only treat stdio as valid open targets. */
        if (path[0] == '/' || path[0] == '.') {
            r->eax = FD_STDOUT;
            break;
        }
        r->eax = -1;
        break;
    }

    case SYS_CLOSE: {
        int fd = (int)a;
        if (fd == FD_STDIN || fd == FD_STDOUT || fd == FD_STDERR) {
            r->eax = 0;
            break;
        }
        r->eax = -1;
        break;
    }

    case SYS_YIELD: {
        /* Cooperative yield: save current pushed register frame into the
         * outgoing process, rotate the ready queue, and modify the saved
         * interrupt return frame so that iret resumes the newly-selected
         * process. */
        struct process *prev = get_curr_proc();
        /* Pointer arithmetic: `r` points at the pusha frame (edi..eax).
         * The saved user EIP is located immediately above the pusha frame
         * on the stack (after 8 32-bit registers). */
        uint32_t *saved_eip_ptr = ((uint32_t *)r) + 8;

        if (prev) {
            /* copy general regs and esp */
            prev->regs = *r;
            prev->esp = r->esp; /* keep process esp in sync */
            prev->saved_eip = *saved_eip_ptr;
            proc_enqueue(prev);
        }

        struct process *next = proc_dequeue();
        if (!next) {
            /* Nothing to run — leave frame as-is; return success */
            r->eax = 0;
            break;
        }

        /* `proc_dequeue` set curr_proc and next->state; restore its saved
         * registers (if any) into the interrupt return frame so iret will
         * resume it. If this is the process' first run, use its entry and
         * esp fields to initialize the frame. */
        if (next->regs.edi || next->regs.eax || next->regs.ebx || next->regs.ecx
            || next->regs.edx || next->regs.ebp || next->regs.esi) {
            /* Has previous regs saved — restore all */
            *r = next->regs;
        } else {
            /* Fresh process: zero general regs, set esp from process. */
            r->edi = 0; r->esi = 0; r->ebp = 0; /* r->esp set below */
            r->ebx = 0; r->edx = 0; r->ecx = 0; r->eax = 0;
            if (next->esp) r->esp = next->esp;
            else if (next->user_stack_top) r->esp = next->user_stack_top;
        }

        /* set the return EIP to the saved one (if available) or to entry */
        if (next->saved_eip) *saved_eip_ptr = next->saved_eip;
        else *saved_eip_ptr = next->entry;

        /* Indicate success to caller */
        r->eax = 0;
        break;
    }

    default:
        r->eax = -1;
        break;
    }
}

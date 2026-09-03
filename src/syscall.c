#include "../include/syscall.h"
#include "../include/screen.h"
#include "../include/paging.h"

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

    default:
        r->eax = -1;
        break;
    }
}

#include "../include/syscall.h"
#include "../include/screen.h"
#include "../include/paging.h"
#include <stdint.h>

extern volatile uint32_t syscall_exit_requested;
volatile uint32_t last_exit_status = 0;

static int validate_user_ptr(const void *ptr, uint32_t len) {
    if (!ptr) return 0;
    uint32_t p = (uint32_t)ptr;
    return validate_user_region(p, len);
}

static int copy_from_user(char *dst, const char *user_src, uint32_t len) {
    if (!validate_user_ptr(user_src, len)) return -1;
    for (uint32_t i = 0; i < len; ++i) dst[i] = user_src[i];
    return 0;
}

static int copy_to_user(char *user_dst, const char *src, uint32_t len) {
    if (!validate_user_ptr(user_dst, len)) return -1;
    for (uint32_t i = 0; i < len; ++i) user_dst[i] = src[i];
    return 0;
}

void handle_syscall(struct regs *r) {
    uint32_t num = r->eax;
    uint32_t a = r->ebx;
    uint32_t b = r->ecx;
    uint32_t c = r->edx;

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
                if (copy_from_user(tmp, user_buf + offset, take) < 0) break;
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
        if (!path || !validate_user_ptr(path, 1)) {
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

# 📋 CIndy-OS — Implementation Plan
> Next steps for syscall completion, fd table, and cooperative scheduler.
> Written: September 2026. Pick up from here next session.

---

## Where We Are Right Now

The kernel is stable. Ring-3 works. The syscall ABI exists. A PCB struct and process
queue are in place. The cooperative yield mechanism is implemented inside the
`SYS_YIELD` handler. The fd table struct is defined but nothing writes into it yet.

**What works today:**
- `SYS_EXIT` — fully working, kernel_return path is stable
- `SYS_WRITE` — works for fd 1/2 (stdout/stderr), FAT16 write is a stub
- `SYS_YIELD` — cooperative switch via iret-frame patching, works in syscall.c
- `struct process` + queue (`proc_enqueue`, `proc_dequeue`, `get_curr_proc`) — done
- `struct fd_entry` + `fds[PROC_MAX_FDS]` in PCB — defined, nothing fills it yet
- `task_create_from_elf` — implemented, reads ELF from FAT16, allocates stack, enqueues PCB
- `spawn` shell command — creates process via task_create_from_elf, calls jump_to_usermode
- `scheduler.c` — `schedule()` and `yield_kernel()` written
- `start_init_proc` — no longer auto-jumps to ring 3 on boot; shell is available immediately

**What is stub / not done:**
- `SYS_OPEN` — returns FD_STDOUT for any path, never fills an fd slot
- `SYS_READ` — returns 0 for stdin, -1 for everything else
- `SYS_CLOSE` — only handles 0/1/2, does not clear FAT16 fd slots
- Timer IRQ preemption — `isr32` does not pass a register frame, so schedule() cannot
  be called from the timer handler safely yet
- Per-process page directories — all processes share the same address space

---

## Step 1 — Complete SYS_OPEN

**File:** `src/syscall.c`, case `SYS_OPEN`

**What to do:**
Replace the stub with real fd-slot allocation:

```c
case SYS_OPEN: {
    const char *path = (const char *)a;
    uint32_t flags = b;
    (void)flags;

    if (!path || (!kernel_mode_call && !validate_user_ptr(path, 1))) {
        r->eax = -1;
        break;
    }

    struct process *proc = get_curr_proc();
    if (!proc) { r->eax = -1; break; }

    /* Find a free slot (skip 0/1/2 — reserved for stdin/stdout/stderr) */
    int fd = -1;
    for (int i = 3; i < PROC_MAX_FDS; i++) {
        if (proc->fds[i].type == FD_TYPE_NONE) {
            fd = i;
            break;
        }
    }
    if (fd == -1) { r->eax = -1; break; }  /* too many open files */

    /* copy path from user space into a kernel buffer */
    char kpath[32];
    if (kernel_mode_call) {
        int j = 0;
        while (path[j] && j < 31) { kpath[j] = path[j]; j++; }
        kpath[j] = '\0';
    } else {
        if (copy_from_user(kpath, path, 32) < 0) { r->eax = -1; break; }
    }

    /* convert to FAT 8.3 */
    char name8[9], ext3[4];
    fat16_format_83(kpath, name8, ext3);

    /* verify the file exists — try a zero-length read */
    uint8_t probe[1];
    if (fat16_read_file(name8, ext3, probe, 1) < 0) {
        r->eax = -1;  /* file not found */
        break;
    }

    /* fill the fd slot */
    proc->fds[fd].type   = FD_TYPE_FAT16;
    proc->fds[fd].offset = 0;
    for (int j = 0; j < 9; j++) proc->fds[fd].name8[j] = name8[j];
    for (int j = 0; j < 4; j++) proc->fds[fd].ext3[j]  = ext3[j];

    r->eax = fd;
    break;
}
```

**Headers needed:** add `#include "../include/fat16.h"` to `syscall.c`.

---

## Step 2 — Complete SYS_READ

**File:** `src/syscall.c`, case `SYS_READ`

**What to do:**
Replace the stub with a real FAT16 read that respects `fd_entry.offset`:

```c
case SYS_READ: {
    int fd = (int)a;
    char *user_buf = (char *)b;
    uint32_t len = c;

    if (fd == FD_STDIN) { r->eax = 0; break; }

    struct process *proc = get_curr_proc();
    if (!proc || fd < 0 || fd >= PROC_MAX_FDS) { r->eax = -1; break; }

    struct fd_entry *fde = &proc->fds[fd];
    if (fde->type != FD_TYPE_FAT16) { r->eax = -1; break; }

    /* read the whole file into a kernel buffer */
    uint8_t kbuf[4096];
    int total = fat16_read_file(fde->name8, fde->ext3, kbuf, sizeof(kbuf));
    if (total < 0) { r->eax = -1; break; }

    /* EOF check */
    if (fde->offset >= (uint32_t)total) { r->eax = 0; break; }

    uint32_t available = (uint32_t)total - fde->offset;
    uint32_t take = (len < available) ? len : available;

    if (copy_to_user(user_buf, kbuf + fde->offset, take) < 0) {
        r->eax = -1;
        break;
    }

    fde->offset += take;
    r->eax = take;
    break;
}
```

**Known limitation:** reads the whole file into a 4096-byte kernel buffer every time.
Fine for small files at this stage. A proper implementation would cache the file or
use a streaming read. Defer until the kernel has a real VFS layer.

---

## Step 3 — Complete SYS_CLOSE

**File:** `src/syscall.c`, case `SYS_CLOSE`

Small fix — clear FAT16 fd slots too:

```c
case SYS_CLOSE: {
    int fd = (int)a;

    /* always allow closing stdio fds without error */
    if (fd == FD_STDIN || fd == FD_STDOUT || fd == FD_STDERR) {
        r->eax = 0;
        break;
    }

    struct process *proc = get_curr_proc();
    if (!proc || fd < 0 || fd >= PROC_MAX_FDS) { r->eax = -1; break; }

    if (proc->fds[fd].type == FD_TYPE_NONE) { r->eax = -1; break; }

    /* clear the slot */
    proc->fds[fd].type   = FD_TYPE_NONE;
    proc->fds[fd].offset = 0;
    proc->fds[fd].name8[0] = '\0';
    proc->fds[fd].ext3[0]  = '\0';

    r->eax = 0;
    break;
}
```

---

## Step 4 — Initialize stdio fds in task_create_from_elf

**File:** `src/process.c`, inside `task_create_from_elf` after zeroing the PCB

Every new process should have fd 0/1/2 pre-filled as STDIO so user programs can
write to stdout without calling SYS_OPEN first:

```c
/* pre-populate stdio fds */
p->fds[FD_STDIN].type  = FD_TYPE_STDIO;
p->fds[FD_STDOUT].type = FD_TYPE_STDIO;
p->fds[FD_STDERR].type = FD_TYPE_STDIO;
```

---

## Step 5 — Timer IRQ preemption groundwork

**Goal:** make `isr32` pass its register frame to `timer_handler` so the scheduler
can eventually be called from the timer IRQ — same pattern as `isr128` already does.

**File:** `src/isr.asm`, replace the bare `isr32`:

```nasm
isr32:
    cli
    pusha

    mov eax, esp        ; pass register frame pointer as arg
    push eax
    call timer_handler
    add esp, 4

    popa
    iret
```

**File:** `src/timer.c`, update the signature:

```c
#include "../include/regs.h"

void timer_handler(struct regs *r) {
    (void)r;            /* unused until preemption lands */
    timer_ticks++;
    outb(0x20, 0x20);   /* EOI */
}
```

**File:** `include/timer.h`:

```c
#include "regs.h"
void init_timer(unsigned int freq);
void timer_handler(struct regs *r);
```

This change alone does nothing visible but it unblocks Step 6.

---

## Step 6 — Conservative timer-driven scheduling (optional, after Step 5)

Once Step 5 is done, add a tick-based yield inside `timer_handler`:

```c
#define SCHED_TICK_INTERVAL 100   /* yield every 100 ticks = ~1 second at 100 Hz */

void timer_handler(struct regs *r) {
    timer_ticks++;
    outb(0x20, 0x20);

#ifdef PREEMPT_ENABLE
    if (timer_ticks % SCHED_TICK_INTERVAL == 0) {
        struct process *curr = get_curr_proc();
        struct process *next = proc_next_ready();
        if (curr && next) {
            /* patch the iret frame the same way SYS_YIELD does */
            uint32_t *saved_eip_ptr = ((uint32_t *)r) + 8;
            curr->regs      = *r;
            curr->saved_eip = *saved_eip_ptr;
            proc_enqueue(curr);

            struct process *n = proc_dequeue();
            if (n->saved_eip) *saved_eip_ptr = n->saved_eip;
            else              *saved_eip_ptr = n->entry;
            if (n->esp) r->esp = n->esp;
        }
    }
#endif
}
```

Guard with `#ifdef PREEMPT_ENABLE` — only activate once you have verified cooperative
switching is stable with two user tasks.

---

## Step 7 — Write a two-task test program

Create `user/task_a.asm` and `user/task_b.asm` to verify cooperative switching:

```nasm
; user/task_a.asm — prints "A" then yields in a loop
[BITS 32]
[GLOBAL _start]
section .text
_start:
.loop:
    mov eax, 1          ; SYS_WRITE
    mov ebx, 1          ; stdout
    mov ecx, msg
    mov edx, 2
    int 0x80
    mov eax, 5          ; SYS_YIELD
    int 0x80
    jmp .loop
section .rodata
msg: db "A", 0x0A
```

```nasm
; user/task_b.asm — prints "B" three times then exits
[BITS 32]
[GLOBAL _start]
section .text
_start:
    mov ecx, 3
.loop:
    push ecx
    mov eax, 1
    mov ebx, 1
    mov ecx, msg
    mov edx, 2
    int 0x80
    mov eax, 5          ; SYS_YIELD
    int 0x80
    pop ecx
    loop .loop
    mov eax, 0          ; SYS_EXIT
    xor ebx, ebx
    int 0x80
section .rodata
msg: db "B", 0x0A
```

Expected serial output when both are running:
```
A
B
A
B
A
B
A
A
A
...
```

---

## Step 8 — ELF .bss zeroing (Week 6 item)

Already done in `load_elf_program` — the `memset` after `memcpy` handles BSS.
Verify by checking the condition:
```c
if (seg_memsz > seg_filesz) {
    memset((void *)(seg_vaddr + seg_filesz), 0, seg_memsz - seg_filesz);
}
```
This is correct. No change needed.

---

## Order of attack

1. Step 1 (SYS_OPEN) — unblocks file reading from user space
2. Step 2 (SYS_READ) — lets user programs actually read files
3. Step 3 (SYS_CLOSE) — cleanup, small
4. Step 4 (stdio fds in task_create) — makes spawn + write work without SYS_OPEN
5. Step 7 (two-task test) — validates cooperative scheduler before touching timer
6. Step 5 (isr32 frame pass) — groundwork for preemption
7. Step 6 (timer preemption) — only after Step 7 is proven stable

---

## Known risks to watch

- **Stack collision between two processes:** both currently use `0x80000FFCu` as
  `user_stack_top`. When you run two processes they will share the same stack page
  and corrupt each other. Fix: allocate a unique stack page per process in
  `task_create_from_elf` — use a different virtual address for each
  (e.g. `0x80000000 + pid * 0x1000`).

- **ELF segment overlap:** two ELF processes loaded from the same binary will share
  the same virtual pages (`0x08048000`). Fine for one-at-a-time execution. Will break
  when running two ELF processes simultaneously. Fix requires per-process CR3 (stretch goal).

- **kmalloc has no free:** every `task_create_from_elf` call leaks a PCB-sized
  allocation. Fine for a handful of processes in a toy OS. Not a blocker.

- **SYS_READ reads the whole file every call:** the 4096-byte kernel buffer is stack-
  allocated inside `handle_syscall`. Keep file sizes under 4096 bytes for now.

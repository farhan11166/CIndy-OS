# CIndy-OS — Project Recap

This project is a bare-metal x86 OS built from scratch in C + NASM. The goal is not to be a full production kernel. The goal is to understand how a kernel actually boots, manages memory, handles interrupts, and loads user programs.

If the codebase feels large, this file is the map.

---

## 1. The big picture

The boot flow is:

1. BIOS/UEFI starts
2. GRUB loads the ISO and kernel
3. GRUB finds the Multiboot header and jumps to `start` in `boot.asm`
4. `boot.asm` loads a flat GDT and switches into protected mode
5. `kernel_main()` runs in C
6. The kernel initializes hardware and subsystems
7. The shell starts and waits for commands

This sequence is the core idea behind the project.

---

## 2. The first layers: boot + GDT + memory

### `src/boot.asm`
- Defines the Multiboot header
- Sets up the kernel stack
- Loads the GDT
- Jumps into 32-bit protected mode
- Calls `kernel_main()`

### `src/gdt.c`
- Writes the GDT entries
- Sets up the TSS descriptor
- Handles ring-3 transitions

### Why this matters
Without a valid GDT and TSS, the CPU has no correct segment layout and cannot safely switch between kernel and user privileges.

This is one of the most important debug bugs in the project: a malformed TSS descriptor caused a page fault during ring changes.

---

## 3. Interrupts and CPU exceptions

### `src/idt.c`
- Builds the IDT table
- Fills each entry with selector, flags, and handler addresses

### `src/isr.asm`
- Defines the CPU exception stubs for interrupts 0-31
- Defines IRQ handlers for timer and keyboard
- Bridges assembly interrupt entry to C handlers

### `src/pic.c`
- Remaps the 8259 PIC
- Moves IRQs away from the CPU exception vectors so they don’t collide

### `src/interrupts.asm`
- Wraps `sti` so the kernel can enable interrupts cleanly

### Why this matters
A kernel without interrupts is just a frozen program. This project learns how the CPU, PIC, and OS interact during hardware events.

---

## 4. Memory management

### `src/memory.c`
This file contains the early memory allocator and the physical memory manager.

Two important ideas here:
- bump allocator: simple and fast for early boot allocations
- bitmap PMM: tracks free 4 KB physical frames for later allocations

### `src/paging.c`
This is the x86 MMU layer.

Key responsibilities:
- allocate page directory and page tables
- identity-map RAM so virtual == physical for the mapped region
- map new pages with `map_page()`
- flush the TLB with `invlpg`

### Why this matters
Paging is what makes memory protection and user-mode execution possible. It is also the most dangerous subsystem: one wrong mapping or one stale TLB entry can produce a page fault.

---

## 5. Filesystems and boot data

### `fs/` and initrd
The kernel can load a TAR archive from GRUB as an initrd and then parse it with a USTAR reader.

### `src/fs.c`
- Parses the initrd archive
- Reads files stored in the initial ramdisk
- Powers `ls` and `cat`

### `src/fat16.c`
- Reads the BPB from disk
- Computes FAT and root directory locations
- Reads and writes files through FAT16 cluster chains
- Implements 8.3 filename conversion

### `src/ata.c`
- Handles ATA PIO disk I/O
- Reads and writes raw sectors

### Why this matters
The OS needs real storage to load programs, files, and user data. This is how the project moves from a boot-only kernel to a usable environment.

---

## 6. The shell and user programs

### `src/keyboard.c`
This is the command shell and the main user interaction layer.

It does:
- read keyboard scancodes
- convert them to ASCII
- build an input buffer
- parse `argc` / `argv`
- dispatch commands like `help`, `echo`, `ls`, `run`, `fat-cat`, etc.

### `user/YOOO.asm`
This is a tiny flat binary that writes an `X` to VGA memory and exits.

The `run` command in the shell loads this file into a mapped memory page and jumps to it.

### Why this matters
This is the “user program” step of the operating system. It demonstrates how a kernel can launch a program and then regain control after it exits.

---

## 7. The most important debugging lessons

These are the bug classes you will keep hitting in a kernel project:

### A. GDT/TSS bugs
A broken TSS or a wrong selector can crash the CPU during ring transitions.

### B. Stack bugs
Kernel/user transitions are tricky. If you save the wrong stack pointer or restore the wrong stack, the CPU will fault or return to garbage state.

### C. Paging bugs
If a physical address is not identity-mapped or a page table is not installed correctly, all memory accesses can fault.

### D. TLB bugs
When mapping a new page, you often need `invlpg` to flush stale cached translations.

### E. Storage bugs
FAT16 and ATA are easy to get wrong if your LBA calculations or cluster-chaining logic are off by one.

---

## 8. Where to look when something breaks

If the OS crashes, the best places to inspect are:

- `src/boot.asm` — boot and GDT setup
- `src/gdt.c` — TSS / segment configuration
- `src/isr.asm` — interrupt and exit stubs
- `src/kernel.c` — main boot flow and syscall/user-mode actions
- `src/paging.c` — page mappings and TLB invalidation
- `src/keyboard.c` — command dispatch and `run` loader
- `src/fat16.c` — file lookup and cluster logic

This is the fastest way to reason about the project without getting lost in the codebase.

---

## 9. The real lesson of the project

CIndy-OS is not supposed to be a giant OS. It is a learning kernel.

Its real value is that it forces you to understand:
- how BIOS and GRUB hand control to the kernel
- how the CPU sees memory in protected mode
- how interrupts actually work
- how a kernel writes to hardware directly
- how paging enables an OS to control memory safely
- how user processes can be loaded and executed

That is the real workflow: build small systems, understand them deeply, and fix them one layer at a time.

---

## 10. Suggested next steps

If you want to continue this project, the natural next moves are:

1. clean system calls (`int 0x80` or `syscall` ABI)
2. separate user pages and kernel pages
3. actual process structure and scheduler
4. ELF loading instead of raw flat binaries
5. a real memory allocator and free-list manager

That is the next logical ladder after this kernel foundation.

# 🖥️ CIndy-OS

CIndy-OS is a small 32-bit x86 hobby operating system built from scratch with NASM and freestanding C. It is intentionally explicit and educational: the boot path, interrupt handling, memory management, paging, filesystems, user-mode execution, system calls, and early process support are implemented by hand.

> **Status:** early kernel / learning OS. The current tree boots with GRUB, provides an interactive shell, accesses a FAT16 disk and initrd, loads flat binaries and simple ELF executables, enters ring 3, and contains the first pieces of a cooperative process model.

---

## 🎬 Demo

https://github.com/user-attachments/assets/REPLACE_WITH_YOUR_VIDEO_ASSET_ID

> Boot → kernel shell → FAT16 disk access → user-mode flat binary (`run YOOO.BIN`) → ring-3 syscall → ELF process spawn.

---

## Current capabilities

### Boot and kernel initialization

- GRUB Multiboot boot path for a 32-bit kernel ISO.
- Protected-mode startup with a GDT, TSS, and kernel stack.
- VGA text-mode output with cursor control, colored output, scrolling, and serial initialization.
- QEMU run targets for normal, curses, and debug-console operation.

### Interrupts and hardware

- IDT setup with CPU exception handlers.
- PIC remapping.
- IRQ0 timer support through the PIT.
- IRQ1 PS/2 keyboard support.
- `int 0x80` syscall entry from user mode.
- Basic keyboard-controller reboot support.

### Interactive kernel shell

The shell supports simple whitespace-separated `argc`/`argv` parsing and currently includes:

- `help`, `clear`, `echo`
- `about`, `version`, `whoami`, `pwd`
- `timer`, `uptime`, `meminfo`
- `ls`, `cat` for the initrd filesystem
- `fat-ls`, `fat-cat`, `fat-write` for the FAT16 disk
- `read-test`, `write-test` for raw ATA sector testing
- `run <file>` to load a flat binary or ELF image
- `spawn <file.elf>` to create and start an ELF-backed process
- `reboot`

### Memory management and paging

- Kernel heap allocation through `kmalloc_a`.
- Physical memory manager using a bitmap of page frames.
- Multiboot memory-map based memory initialization.
- Paging enabled with identity-mapped physical memory sized from detected RAM, with a fallback mapping when detection is unavailable.
- Dynamic page mapping through `map_page` and page-table lookup through `get_pte`.
- User-region and user-pointer validation for syscall memory access.
- User mappings currently use a shared page directory and are not isolated per process.

### Storage and filesystems

- ATA PIO sector reads and writes.
- A generated 20 MiB FAT16 disk image.
- FAT16 root-directory listing, file reads, and file creation/writes using 8.3 filenames.
- USTAR/tar-style initrd support for listing and reading files.
- The build copies the generated initrd into the bootable ISO and test programs into the FAT16 disk image.

### User-mode execution

- Ring-3 entry and return helpers using the GDT/TSS setup.
- Flat binaries can be loaded at `0x40000000`.
- Simple 32-bit `ET_EXEC` ELF files are recognized and their `PT_LOAD` segments are mapped at their requested virtual addresses.
- A user stack is allocated and mapped at `0x80000000`.
- `SYS_EXIT` returns control to the kernel-side launch path.
- The repository includes `user/YOOO.asm` as a flat-binary syscall test and `user/hello_elf.asm` as a minimal ELF example.

### Process foundation

The repository now contains an initial process subsystem:

- Process states: `NEW`, `READY`, `RUNNING`, `BLOCKED`, and `EXITED`.
- PID allocation and a FIFO ready queue.
- Process lookup by PID and current-process tracking.
- `spawn <file.elf>` creates a process structure, loads an ELF image, allocates a user stack, and queues it.
- `SYS_YIELD` rotates the ready queue and saves/restores the basic register frame.

This is process and scheduling groundwork rather than complete multitasking: context switching, address-space isolation, and lifecycle cleanup are still incomplete.

---

## System-call ABI

System calls are entered through `int 0x80` with the following register convention:

- `eax` — syscall number
- `ebx` — first argument
- `ecx` — second argument
- `edx` — third argument

Defined calls:

| Number | Name | Current behavior |
|---:|---|---|
| `0` | `SYS_EXIT` | Records the exit status and requests return from the user program. |
| `1` | `SYS_WRITE` | Writes to stdout/stderr; other file descriptors are not fully implemented. |
| `2` | `SYS_OPEN` | Minimal placeholder behavior; only limited paths are accepted. |
| `3` | `SYS_CLOSE` | Accepts the standard descriptors; other descriptors are not complete. |
| `4` | `SYS_READ` | stdin currently returns zero; general file reads are not implemented. |
| `5` | `SYS_YIELD` | Performs an early cooperative ready-queue rotation when a process is active. |

Standard descriptors are `0` for stdin, `1` for stdout, and `2` for stderr. User pointers are validated against mapped, user-accessible pages before copying data, except for the temporary kernel-mode test fallback used by early execution paths.

---

## Project structure

```text
CIndy-OS/
├── src/
│   ├── boot.asm              # Multiboot entry and early startup
│   ├── kernel.c              # Kernel initialization and interrupt registration
│   ├── screen.c              # VGA text output and serial helpers
│   ├── ports.c               # x86 I/O port wrappers
│   ├── idt.c                 # IDT construction
│   ├── idt_load.asm          # lidt helper
│   ├── isr.asm               # exception, IRQ, and syscall stubs
│   ├── interrupts.asm        # interrupt enable helper
│   ├── pic.c                 # PIC remapping
│   ├── timer.c               # PIT timer support
│   ├── keyboard.c            # PS/2 keyboard, shell, binary and ELF loading
│   ├── memory.c              # Kernel allocator and physical memory manager
│   ├── paging.c              # Page directories, mapping, and validation
│   ├── fs.c                  # Initrd/USTAR filesystem
│   ├── ata.c                 # ATA PIO disk access
│   ├── fat16.c               # FAT16 driver
│   ├── gdt.c                 # GDT and TSS setup
│   ├── syscall.c             # Syscall dispatch and user copies
│   ├── process.c             # Early process and ready-queue support
│   ├── string.c              # Freestanding string/memory helpers
│   ├── gdt_flush.asm         # GDT reload helper
│   └── usermode.asm          # Ring-3 entry/return helpers
├── include/                  # Kernel headers and ABI definitions
├── user/
│   ├── YOOO.asm              # Flat user-mode syscall test
│   └── hello_elf.asm         # Minimal 32-bit ELF test program
├── fs/                       # Files packaged into the initrd
├── iso/boot/grub/grub.cfg    # GRUB configuration
├── linker.ld                 # Kernel linker script
├── Makefile                  # Build, image, ISO, and QEMU targets
├── disk.img                  # Generated FAT16 disk image
├── recap.md                  # Project workflow recap
├── docs/                     # Learning notes, internals, devlog, and errors
└── README.md
```

Generated artifacts such as `kernel.bin`, `CIndy-os.iso`, `initrd.tar`, and the FAT16 image are produced or refreshed by the build process and may be present in the working tree.

---

## Runtime flow

### Boot flow

```text
GRUB -> kernel.bin -> boot.asm -> kernel_main()
```

Kernel startup initializes the serial console, screen, GDT/TSS, IDT, PIC, timer, paging, physical memory manager, initrd, FAT16 driver, and process subsystem before entering the keyboard-driven shell.

### Program flow

```text
run <file>
  -> read a file from FAT16
  -> detect a flat binary or ELF image
  -> allocate physical memory and map user pages
  -> allocate/map a user stack
  -> enter ring 3
  -> user code invokes int 0x80
  -> syscall handler validates arguments and dispatches the call
  -> SYS_EXIT returns to the kernel launcher
```

### Process flow

```text
spawn <file.elf>
  -> read and validate an ELF image
  -> map its PT_LOAD segments
  -> allocate a user stack
  -> create a process structure and PID
  -> enqueue it as READY
  -> dequeue and enter its user entry point
```

---

## Build and run

### Prerequisites

On Debian/Ubuntu-like systems:

```bash
sudo apt-get install -y nasm gcc grub-pc-bin grub-common xorriso mtools qemu-system-x86
```

A working 32-bit freestanding toolchain is required because the kernel is compiled with `gcc -m32` and linked as `elf_i386`.

### Build

```bash
make
```

The Makefile:

1. assembles the boot, interrupt, GDT, and user-mode assembly;
2. compiles the freestanding C kernel sources;
3. links `kernel.bin`;
4. builds the flat-binary and ELF user tests;
5. creates a 20 MiB FAT16 `disk.img` and copies the user images into it;
6. packages `fs/` as `initrd.tar`; and
7. creates `CIndy-os.iso` with `grub-mkrescue`.

### Run in QEMU

```bash
make run
make run-curses
make run-debug
```

`make run-debug` hides the graphical display and sends the QEMU debug console to standard output.

### Clean generated output

```bash
make clean
```

---

## Known limitations

CIndy-OS is not yet a general-purpose operating system. Current limitations include:

- one shared page directory; no per-process address spaces or CR3 switching;
- no complete scheduler or timer-driven preemptive multitasking;
- cooperative yield support is present but still an early context-switching prototype;
- process kernel stacks, cleanup, termination, and complete saved CPU state are unfinished;
- file descriptors are only partially modeled and filesystem-backed syscall I/O is incomplete;
- `SYS_READ` is not connected to a keyboard input buffer;
- `SYS_OPEN` and `SYS_CLOSE` have placeholder semantics;
- ELF loading supports a small 32-bit `ET_EXEC` subset and does not provide relocations, dynamic linking, or full permission handling;
- user memory isolation is limited by the shared identity-mapped address space;
- the kernel allocator has no general free path, so some temporary load buffers are leaked;
- hardware support is intentionally limited to the devices needed by the current learning path.

These constraints are deliberate milestones for an educational kernel, not claims of production readiness.

---

## Roadmap

- [x] GRUB boot and protected mode
- [x] GDT, TSS, IDT, PIC, timer, and keyboard interrupt setup
- [x] Kernel shell and basic diagnostics
- [x] Physical memory manager and paging
- [x] Initrd/USTAR and FAT16 read/write support
- [x] Flat-binary and minimal ELF loading
- [x] Ring-3 transition and basic syscall entry
- [x] Initial process structures, PID allocation, ready queue, and cooperative yield prototype
- [ ] Per-process page directories and address-space isolation
- [ ] Complete process context switching and kernel stacks
- [ ] Timer-driven preemptive scheduler
- [ ] Process exit/reaping and resource cleanup
- [ ] Proper file-descriptor table and filesystem-backed syscalls
- [ ] Keyboard input buffering for `SYS_READ`
- [ ] More complete ELF validation, permissions, and relocation support

---

## Learning resources

- [OSDev Wiki](https://wiki.osdev.org/)
- [Multiboot Specification](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html)
- [NASM Documentation](https://nasm.us/doc/)
- [James Molloy's Kernel Tutorial](http://www.jamesmolloy.co.uk/tutorial_html/)
- [x86 Instruction Reference](https://www.felixcloutier.com/x86/)

---

## License

This is a personal learning project. Use and modify freely.

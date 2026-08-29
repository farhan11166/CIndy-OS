# 🖥️ CIndy-OS

A bare-metal x86 hobby operating system built from scratch in NASM and C. The project is intentionally small, explicit, and educational: each subsystem is implemented by hand so the kernel can be understood step by step.

---

## What is working right now

CIndy-OS now has a stable protected-mode kernel with a working ring-3 transition path, a usable minimal syscall ABI, and a primitive ELF loader. The design is still intentionally simple, but it is already enough to load a user program, switch into ring 3, make syscalls, and return cleanly to the kernel.

Current capabilities:

1. **Boot and protected mode**
   - GRUB boots the kernel from ISO
   - GDT and TSS are initialized and ring transitions are supported
   - kernel enters protected mode and sets up a stable stack

2. **Interrupts and exceptions**
   - IDT is configured
   - PIC is remapped
   - IRQ0 and IRQ1 are handled
   - CPU exceptions are caught without silently crashing

3. **Kernel shell**
   - command parser with `argc/argv`
   - built-in shell commands such as `help`, `about`, `version`, `echo`, `ls`, `cat`, `meminfo`, `timer`, `uptime`, `run`, etc.

4. **Memory and paging**
   - PMM tracks physical frames via bitmap allocation
   - paging is enabled with an identity map for the lower memory region
   - page validation checks for user pages before allowing user-space access

5. **Filesystem support**
   - initrd/USTAR support for basic file listing and reading
   - FAT16 disk access for file loading and writing

6. **User-mode execution**
   - kernel can load a program into a mapped virtual region and jump to ring 3
   - a user program can execute in ring 3 and return to the kernel via a syscall exit path

7. **Minimal syscall ABI**
   - `SYS_EXIT`
   - `SYS_WRITE`
   - `SYS_READ`
   - `SYS_OPEN`
   - `SYS_CLOSE`
   - current implementation is intentionally small and safe for the learning project stage

8. **ELF loading**
   - the `run` command detects ELF headers and loads PT_LOAD segments
   - this is a minimal loader, not a full ELF runtime or relocator yet

---

## Project structure

```
CIndy-OS/
├── src/
│   ├── boot.asm              # entry point, multiboot header, early GDT setup
│   ├── kernel.c              # main kernel logic, process of init and shell
│   ├── screen.c              # VGA text output, cursor, scrolling
│   ├── ports.c               # inb/outb wrappers
│   ├── idt.c                 # IDT setup
│   ├── idt_load.asm          # lidt helper
│   ├── isr.asm               # ISR/IRQ stubs, syscall entry logic
│   ├── interrupts.asm        # enable_interrupts()
│   ├── pic.c                 # PIC remapping
│   ├── timer.c               # PIT / timer tick logic
│   ├── keyboard.c            # PS/2 keyboard, shell, run command, primitive ELF loader
│   ├── memory.c              # kmalloc + PMM
│   ├── paging.c              # page tables, user validation, map_page
│   ├── fs.c                  # initrd / tar-like fs access
│   ├── fat16.c               # FAT16 driver
│   ├── string.c              # basic string functions
│   ├── ata.c                 # ATA PIO disk access
│   ├── gdt.c                 # GDT/TSS setup
│   ├── syscall.c             # minimal syscall implementation
│   └── usermode.asm          # ring-3 entry/exit helpers
├── include/
│   ├── types.h
│   ├── screen.h
│   ├── ports.h
│   ├── idt.h
│   ├── interrupts.h
│   ├── pic.h
│   ├── timer.h
│   ├── keyboard.h
│   ├── memory.h
│   ├── paging.h
│   ├── fs.h
│   ├── fat16.h
│   ├── string.h
│   ├── ata.h
│   ├── multiboot.h
│   ├── regs.h                # register frame definition for syscall handling
│   └── syscall.h             # syscall numbers + prototypes
├── user/
│   ├── YOOO.asm              # simple user-mode test using SYS_WRITE + SYS_EXIT
│   └── hello_elf.asm        # minimal ELF example
├── iso/
│   └── boot/
│       ├── kernel.bin        # generated kernel image
│       ├── initrd.tar        # generated initrd
│       └── grub/
│           └── grub.cfg      # boot config
├── fs/
│   └── files for initrd
├── disk.img                  # FAT16 virtual disk
├── linker.ld                 # kernel linker script
├── Makefile                  # build + run targets
├── recap.md                  # project recap / high-level workflow map
├── docs/
│   ├── LEARNING.md
│   ├── INTERNALS.md
│   ├── DEVLOG.md
│   └── ERRORS.md
├── README.md                 # this file
├── CIndy-os.iso              # generated bootable ISO
└── initrd.tar                # generated archive for boot
```

---

## The actual system design in one flow

### Boot flow

```
BIOS/UEFI -> GRUB -> kernel.bin -> boot.asm -> kernel_main()
```

Kernel startup does:
- set up GDT
- set up IDT
- remap PIC
- initialize timer and keyboard
- parse memory map
- initialize PMM
- enable paging
- mount/initrd and FAT16
- start shell loop

### User-mode flow

```
run <file>
  -> read file from FAT16
  -> detect flat binary or ELF
  -> allocate user page(s)
  -> map them in virtual memory
  -> set up a user stack
  -> jump_to_usermode(user_stack, entry)
  -> user code executes in ring 3
  -> int 0x80 triggers syscall
  -> kernel handles it
  -> SYS_EXIT restores kernel stack and returns to host C code
```

This is the core learning path of the project: boot -> kernel -> memory -> paging -> user mode -> syscall -> back to kernel.

---

## Minimal syscall model

The current syscall layer is deliberately simple and intentionally small:

- `eax` = syscall number
- `ebx` = arg1
- `ecx` = arg2
- `edx` = arg3

Supported minimal calls:
- `SYS_EXIT` (0)
- `SYS_WRITE` (1)
- `SYS_READ` (4)
- `SYS_OPEN` (2)
- `SYS_CLOSE` (3)

This is enough to support a basic user program that prints text and exits cleanly.

---

## Important limitations

This project is not yet a full OS in the modern sense. It is still a learning OS. Important current limits:

- no full scheduler yet
- no per-process page directory switching
- no true process descriptor table
- no full user-space memory isolation beyond basic validation
- `SYS_READ` and `SYS_OPEN` are still minimal stubs
- ELF loading is minimal and does not yet do relocation-heavy advanced ELF handling

This is okay and expected for the current phase.

---

## Build and run

### Prerequisites

```bash
sudo apt-get install -y nasm gcc grub-pc-bin grub-common xorriso mtools qemu-system-x86
```

### Build

```bash
make
```

Build steps:
1. assemble assembly sources
2. compile C sources with 32-bit freestanding flags
3. link kernel.bin
4. build test user programs
5. place them into disk.img
6. build ISO for booting

### Run

```bash
make run
make run-curses
make run-debug
```

---

## Roadmap

### Completed
- [x] GDT + protected mode
- [x] TSS and ring-3 transition fix
- [x] IDT and hardware interrupt setup
- [x] keyboard + shell
- [x] PMM and paging
- [x] initrd / FAT16 support
- [x] user-mode execution
- [x] minimal syscall layer
- [x] minimal ELF loader

### Next real milestones
- [ ] per-process page directory model
- [ ] real process structs / task state
- [ ] cooperative scheduler
- [ ] proper file descriptor table
- [ ] richer syscall semantics and return values
- [ ] preemptive multitasking

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

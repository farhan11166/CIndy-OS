# 🖥️ CIndy-OS

A bare-metal x86 hobby operating system that boots via GRUB, written from scratch in NASM assembly and C.

---

## 📸 What it does right now

When booted, CIndy-OS initializes its own GDT, sets up the IDT, remaps the Programmable Interrupt Controller (PIC), and provides a fully interactive environment:
1. **Hardware Interrupts**: Handles timer ticks (IRQ0) and keyboard input (IRQ1).
2. **CPU Exceptions**: Catches and handles 32 different CPU exceptions (like Divide by Zero and Page Faults) preventing silent reboots.
3. **Interactive Shell**: Features a command-line interface with an `argc/argv` parser.
4. **Built-in Commands**: Try typing `help`, `about`, `version`, `timer`, `echo`, `meminfo`, or `reboot`!
5. **Memory Management**: Parses the GRUB Multiboot memory map and provides a dynamic bump allocator (`kmalloc`).
6. **Read-Only File System**: Uses an Initial Ramdisk (initrd) formatted as a USTAR archive to support file reading (`ls`, `cat`).

All output is written directly to the VGA text buffer at physical address `0xB8000`, and input is processed directly from the PS/2 controller port `0x60`.

---

## 🗂️ Project Structure

```
CIndy-OS/
├── src/                      # Core kernel source files
│   ├── boot.asm              # Assembly entry point + Multiboot header + GDT setup
│   ├── kernel.c              # Main kernel entry point (kernel_main)
│   ├── screen.c              # VGA text buffer, printing, cursor control
│   ├── ports.c               # Port I/O wrappers (inb, outb)
│   ├── idt.c                 # Interrupt Descriptor Table setup
│   ├── idt_load.asm          # Assembly routine to load IDT register
│   ├── isr.asm               # CPU exception handlers (ISR 0-31)
│   ├── interrupts.asm        # Hardware interrupt stubs
│   ├── pic.c                 # Programmable Interrupt Controller remapping
│   ├── timer.c               # Programmable Interval Timer (PIT) driver
│   ├── keyboard.c            # PS/2 keyboard driver + input parsing
│   ├── memory.c              # Memory management: bump allocator (kmalloc)
│   ├── fs.c                  # USTAR archive filesystem driver (ls, cat)
│   └── string.c              # String utility functions
├── include/                  # Header files
│   ├── types.h               # Type definitions (uint8_t, uint32_t, etc.)
│   ├── screen.h              # Screen interface
│   ├── ports.h               # Port I/O interface
│   ├── idt.h                 # IDT structures and functions
│   ├── interrupts.h          # Interrupt declarations
│   ├── pic.h                 # PIC interface
│   ├── timer.h               # Timer interface
│   ├── keyboard.h            # Keyboard interface
│   ├── memory.h              # Memory management interface
│   ├── fs.h                  # Filesystem interface
│   ├── string.h              # String utilities
│   └── multiboot.h           # Multiboot info structure
├── iso/                      # ISO boot configuration
│   └── boot/
│       ├── kernel.bin        # Compiled kernel (generated)
│       ├── initrd.tar        # Initial ramdisk (generated)
│       └── grub/
│           └── grub.cfg      # GRUB bootloader menu
├── fs/                       # Filesystem content (packed into initrd.tar)
├── linker.ld                 # Linker script – places kernel at 1 MiB
├── Makefile                  # Build & run targets
├── README.md                 # This file
└── CIndy-os.iso              # Bootable ISO output (generated)
```

---

## ⚙️ How It Works

### Boot Flow

```
BIOS/UEFI
   ↓
GRUB (reads CIndy-os.iso)
   ↓ loads kernel.bin
searches for Multiboot header in .multiboot section
   ↓
jumps to start (boot.asm)
   ↓
   - Loads GDT for protected mode
   - Sets up stack (16 KiB)
   ↓
calls kernel_main() (kernel.c)
   ↓
   - Initializes IDT + CPU exception handlers
   - Remaps PIC
   - Enables hardware interrupts
   - Initializes memory manager
   - Loads initrd filesystem
   ↓
enters interactive shell loop
   ↓ (on halt)
cli + hlt loop (halts CPU)
```

### Key Files Explained

#### `src/boot.asm` — Assembly Entry Point & GDT Setup
- **Multiboot Header**: Defines magic (`0x1BADB002`), flags, checksum for GRUB
- **GDT (Global Descriptor Table)**: Sets up flat memory model with code and data segments for 32-bit protected mode
- **Stack Setup**: Allocates 16 KiB stack in `.bss` section
- **ESP Register**: Points stack pointer to top of stack before calling kernel
- **Kernel Call**: Jumps to `kernel_main()` in C, preserving Multiboot parameters (magic, multiboot_info*)
- **Infinite Loop**: Returns to `cli` + `hlt` if kernel ever returns

#### `src/kernel.c` — Main Kernel Entry Point
- `kernel_main(unsigned int magic, struct multiboot_info* mbd)` — Main entry point
  - Initializes screen (clears, enables cursor)
  - Sets up IDT with 32 CPU exception handlers
  - Remaps PIC to reroute hardware interrupts
  - Enables interrupts globally
  - Initializes timer driver
  - Parses Multiboot memory map and initializes bump allocator
  - Loads initial ramdisk (initrd) from GRUB module
  - Enters interactive shell loop with `hlt` instruction

#### `src/screen.c` — VGA Text Buffer & Output
- `clear_screen()` — Clears 80×25 VGA cells to blank
- `print(const char* msg)` — Prints string at current cursor position
- `print_colored(const char* msg, unsigned char color)` — Prints with color attribute
- `print_int(int n)` — Converts and prints integer
- `print_hex(unsigned int n)` — Converts and prints hexadecimal
- `enable_cursor(unsigned char start, unsigned char end)` — Hardware cursor setup
- **Direct Hardware Access**: Writes character + color bytes directly to VGA buffer at `0xB8000`

#### `src/idt.c` — Interrupt Descriptor Table
- `idt_init()` — Allocates and initializes IDT
- `idt_set_gate(int n, unsigned int handler, unsigned char sel, unsigned char flags)` — Registers interrupt handler
  - `sel`: Segment selector (typically `0x08` for code segment)
  - `flags`: `0x8E` for interrupt gate (present + 32-bit)
- Uses assembly routine `idt_load()` to load IDT register via `lidt` instruction

#### `src/isr.asm` — CPU Exception Handlers (ISR 0-31)
- **ISR 0-7**: Fault exceptions (Divide by Zero, Debug, NMI, Breakpoint, Overflow, Bound Range, Invalid Opcode, Device Not Available)
- **ISR 8-15**: Fault/Abort exceptions (Double Fault, Coprocessor, Invalid TSS, Segment Not Present, Stack Segment, General Protection, Page Fault, Floating Point)
- **ISR 16-31**: Other exceptions (FPU Exception, Alignment Check, Machine Check, SIMD)
- Each handler pushes registers, calls C exception handler, and restores state
- Prevents "triple fault" reboots by catching and printing exception info

#### `src/interrupts.asm` — Hardware Interrupt Stubs (IRQ 0-1)
- **IRQ 0 (Timer)**: Calls `timer_handler()` for PIT ticks
- **IRQ 1 (Keyboard)**: Calls `keyboard_handler()` for PS/2 input
- Sends EOI (End Of Interrupt) signal to PIC before returning

#### `src/pic.c` — Programmable Interrupt Controller
- `pic_remap()` — Remaps PIC to avoid conflict with CPU exceptions
  - ICW1-ICW4 initialization sequence via ports `0x20`/`0xA0`
  - Maps IRQ 0-7 → vectors 32-39, IRQ 8-15 → vectors 40-47

#### `src/timer.c` — Programmable Interval Timer (PIT)
- `init_timer(unsigned int freq)` — Sets PIT frequency via port `0x43` + `0x40`
  - Divisor = 1193180 / freq (e.g., 100 Hz → 11931)
- Provides periodic interrupts for time tracking and scheduler hooks

#### `src/keyboard.c` — PS/2 Keyboard Driver
- `handle_keyboard_interrupt(unsigned char scancode)` — Converts PS/2 scancodes to ASCII
  - Handles shift key, special keys (Enter, Backspace)
  - Buffers input for command parsing
- `parse_command(const char* cmd)` — Parses shell commands with `argc`/`argv`
- **Built-in Commands**:
  - `help` — Lists available commands
  - `about` — Prints OS info
  - `version` — Shows kernel version
  - `timer` — Displays timer ticks
  - `echo <text>` — Echoes back text
  - `meminfo` — Shows memory usage
  - `reboot` — Reboots system
  - `ls` — Lists files in initrd
  - `cat <file>` — Reads and displays file contents

#### `src/memory.c` — Memory Management
- `init_memory(struct multiboot_info* mbd, unsigned int magic)` — Parses Multiboot memory map
  - Reads BIOS memory regions from `mbd->mmap_addr` + `mbd->mmap_length`
  - Calculates available RAM
- `void* kmalloc(unsigned int size)` — Bump allocator
  - Allocates contiguous memory from "kernel heap"
  - Simple: just increments a pointer (no free/fragmentation)
  - Suitable for bootloader and static kernel structures

#### `src/fs.c` — USTAR TAR Archive Filesystem
- `init_fs(unsigned int initrd_address)` — Parses USTAR tar archive from initrd
  - Reads file headers and metadata
  - Supports `ls` — lists all files in archive
  - Supports `cat <filename>` — reads and displays file contents
- **Read-Only**: No write operations (stateless)
- **Format**: Standard USTAR tar format, loadable by GRUB as module

#### `src/ports.c` — I/O Port Wrappers
- `unsigned char inb(unsigned short port)` — Reads byte from I/O port
- `void outb(unsigned short port, unsigned char value)` — Writes byte to I/O port
- Uses GCC inline assembly (`in`/`out` instructions)

#### `src/string.c` — String Utilities
- `strlen(const char* str)` — Returns string length
- `strcmp(const char* a, const char* b)` — Compares strings
- `strcpy(char* dest, const char* src)` — Copies string
- Standard C library equivalents for freestanding environment

#### `include/multiboot.h` — Multiboot Info Structure
- Defines `struct multiboot_info` with:
  - Memory map (base address, size, type of each region)
  - Module list (bootloader-loaded files like initrd)
  - Command-line arguments
  - Boot loader name
- Follows GNU Multiboot 1.0 specification

#### `linker.ld` — Linker Script
- Sets kernel load address to **1 MiB** (`0x100000`) — standard for protected-mode kernels
- Ensures `.multiboot` is **first section** in binary (GRUB scans first 8 KiB)
- Aligns sections to 4 KiB page boundaries for MMU compatibility
- Separates `.text` (code), `.rodata` (constants), `.data` (initialized data), `.bss` (uninitialized)

---

## 🛠️ Build & Run

### Prerequisites

```bash
sudo apt-get install -y nasm gcc grub-pc-bin grub-common xorriso mtools qemu-system-x86
```

| Tool | Purpose |
|------|---------|
| `nasm` | Assemble `.asm` files → object files |
| `gcc` | Compile `.c` files → object files (freestanding, 32-bit) |
| `ld` | Link objects into `kernel.bin` using `linker.ld` |
| `grub-mkrescue` | Package kernel + GRUB into a bootable ISO |
| `mtools` | Required by `grub-mkrescue` to create the FAT image |
| `xorriso` | Creates the ISO filesystem |
| `qemu` | Run the ISO in a virtual machine |

### Build

```bash
make
```

This runs the full pipeline:
```
1. Assemble boot.asm, idt_load.asm, isr.asm, interrupts.asm → .o files
2. Compile all .c files (kernel, screen, ports, idt, pic, keyboard, timer, string, memory, fs) → .o files
3. Link all object files using linker.ld → kernel.bin
4. Copy kernel.bin to iso/boot/kernel.bin
5. Create tar archive from fs/ directory → initrd.tar
6. Copy initrd.tar to iso/boot/initrd.tar (GRUB will load this as module)
7. Generate bootable ISO using grub-mkrescue → CIndy-os.iso
```

### Run in QEMU

```bash
make run
# equivalent to:
qemu-system-i386 -cdrom CIndy-os.iso
```

### Debug Options

```bash
make run-curses      # Run with curses (text-based) display
make run-debug       # Run with debug console output
```

---

## 🗺️ Roadmap / Next Steps

### Completed ✅
- [x] GDT (Global Descriptor Table) flat setup
- [x] IDT (Interrupt Descriptor Table) + CPU exception handling (32 exceptions)
- [x] Programmable Interval Timer (PIT) — hardware timer
- [x] PS/2 keyboard driver with scancode-to-ASCII conversion
- [x] Interactive shell with command parsing (`argc`/`argv`)
- [x] Memory management — Multiboot memory parsing + bump allocator
- [x] Read-only filesystem — USTAR tar archive support (`ls`, `cat`)

### Future Enhancements 🎯
- [ ] **Virtual Memory / Paging** — Enable paging for 4 GiB address space and memory protection
- [ ] **Dynamic Memory Management** — Implement malloc/free with fragmentation handling (heap allocator)
- [ ] **Process Management** — Context switching, process scheduler, task switching
- [ ] **File System Writes** — Writable filesystem (FAT12/32 or ext2
- [ ] **Preemptive Multitasking** — Multiple processes running concurrently
- [ ] **System Calls** — User mode vs. kernel mode, syscall interface
- [ ] **Debugging Tools** — GDB support, kernel debugger

### Point of View / Philosophy
CIndy-OS is a **learning project**, not production-grade. The focus is on:
- **Understanding hardware**: Direct hardware interaction (ports, interrupts, memory)
- **Simplicity over efficiency**: Clean, readable code over optimization
- **Hands-on OS concepts**: Paging, multitasking, I/O, memory management
- **Minimal dependencies**: No external bootloaders (GRUB) or kernel frameworks
- **Educational value**: Each component is self-contained and well-documented

The goal is to explore OS kernel design from first principles, not to build a feature-complete or performant OS. Future features will prioritize learning impact over complexity.

---

## 🧠 Learning Resources

- [OSDev Wiki](https://wiki.osdev.org/) — comprehensive OS development reference
- [Multiboot Specification](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html)
- [NASM Documentation](https://nasm.us/doc/) — assembly syntax and directives
- [James Molloy's Kernel Tutorial](http://www.jamesmolloy.co.uk/tutorial_html/) — OS fundamentals
- [x86 Assembly Reference](https://www.felixcloutier.com/x86/) — instruction reference
- [USTAR Tar Format](https://www.gnu.org/software/tar/manual/tar.html#SEC141) — archive format spec

---

## 📄 License

This is a personal learning project. Do whatever you want with it.

# 🖥️ CIndy-OS

A bare-metal x86 hobby operating system that boots via GRUB, written from scratch in NASM assembly and C.

---

## 📸 What it does right now

When booted, CIndy-OS initializes its own GDT, sets up the IDT, remaps the PIC, and provides a fully interactive environment:

1. **Hardware Interrupts**: Handles timer ticks (IRQ0) and keyboard input (IRQ1).
2. **CPU Exceptions**: Catches all 32 CPU exceptions (Divide by Zero, Page Fault, etc.) preventing silent reboots.
3. **Interactive Shell**: Command-line interface with an `argc/argv` parser.
4. **Built-in Commands**: `help`, `about`, `version`, `timer`, `echo`, `meminfo`, `uptime`, `reboot`, `ls`, `cat`, `fat-ls`, `fat-cat`, `fat-write`, `run`, `read-test`, `write-test`
5. **Physical Memory Manager**: Parses the GRUB Multiboot memory map; bitmap allocator tracks every 4 KB frame. Hardware-reserved regions (VGA buffer, BIOS ROM) are permanently marked used.
6. **Virtual Memory / Paging**: Full x86 two-level page table. Identity-maps all of RAM (128 MB) so `map_page()` is safe to use at any time. `invlpg` flushes the TLB on every new mapping.
7. **Program Loader**: `run <file.bin>` allocates a physical frame, maps it to virtual address `0x40000000`, loads a flat binary from the FAT16 disk directly into that address, and calls it as a function.
8. **Read-Only File System**: USTAR Initial Ramdisk (initrd) loaded by GRUB, supports `ls` and `cat`.
9. **FAT16 Read/Write Filesystem**: Full read and write support over the ATA PIO driver. `fat-ls`, `fat-cat`, `fat-write`.
10. **ATA PIO Hard Drive Driver**: 28-bit LBA sector read/write to a virtual IDE disk.

All output is written directly to the VGA text buffer at `0xB8000`. Input is processed from PS/2 port `0x60`.

---

## 🗂️ Project Structure

```
CIndy-OS/
├── src/                      # Core kernel source files
│   ├── boot.asm              # Assembly entry point, Multiboot header, GDT setup
│   ├── kernel.c              # Main kernel entry point (kernel_main)
│   ├── screen.c              # VGA text buffer, printing, cursor, scrolling
│   ├── ports.c               # Port I/O wrappers (inb, outb, inw, outw)
│   ├── idt.c                 # Interrupt Descriptor Table setup
│   ├── idt_load.asm          # Assembly routine to load IDT register (lidt)
│   ├── isr.asm               # CPU exception handlers (ISR 0-31) + IRQ stubs
│   ├── interrupts.asm        # enable_interrupts() wrapper (sti)
│   ├── pic.c                 # 8259 PIC remapping
│   ├── timer.c               # PIT driver + IRQ0 handler
│   ├── keyboard.c            # PS/2 keyboard driver, shell, command dispatcher
│   ├── memory.c              # Bump allocator (kmalloc/kmalloc_a) + bitmap PMM
│   ├── paging.c              # x86 paging: init_paging(), map_page()
│   ├── fs.c                  # USTAR archive filesystem driver (initrd)
│   ├── fat16.c               # FAT16 read/write filesystem driver
│   ├── string.c              # String utility functions (freestanding)
│   └── ata.c                 # ATA PIO disk driver
├── include/                  # Header files
│   ├── types.h               # uint8_t, uint32_t, etc.
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
│   └── multiboot.h           # Multiboot info structure
├── user/                     # User-space programs (flat binaries)
│   └── YOOO.asm              # Test program: writes 'X' to bottom-right VGA cell
├── iso/                      # ISO boot configuration
│   └── boot/
│       ├── kernel.bin        # Compiled kernel (generated)
│       ├── initrd.tar        # Initial ramdisk (generated)
│       └── grub/
│           └── grub.cfg      # GRUB bootloader menu
├── fs/                       # Files packed into initrd.tar
├── disk.img                  # FAT16 formatted virtual hard disk
├── linker.ld                 # Linker script — places kernel at 1 MiB
├── Makefile                  # Build & run targets
└── CIndy-os.iso              # Bootable ISO output (generated)
```

---

## ⚙️ How It Works

### Boot Flow

```
BIOS/UEFI
   ↓
GRUB (reads CIndy-os.iso)
   ↓ loads kernel.bin + initrd.tar as module
searches for Multiboot header in .multiboot section
   ↓
jumps to start (boot.asm)
   ↓
   - Loads GDT (flat 32-bit protected mode)
   - Sets up 16 KiB stack
   ↓
calls kernel_main() (kernel.c)
   ↓
   - Initializes IDT (32 exception handlers + timer + keyboard)
   - Remaps PIC (IRQ 0-7 → vectors 32-39)
   - Enables hardware interrupts
   - Initializes PIT at 100 Hz
   - Parses Multiboot memory map (init_memory)
   - Initializes bitmap PMM (init_pmm) — only frees type-1 RAM regions
   - Enables paging (identity-maps 128 MB)
   - Loads initrd filesystem
   - Initializes FAT16 driver
   ↓
enters interactive shell loop
   ↓ (on halt)
cli + hlt loop
```

### Key Subsystems

#### Memory Management (`memory.c`)
Two allocators work together:

- **Bump allocator** (`kmalloc` / `kmalloc_a`): used during early boot for kernel structures. `kmalloc_a` aligns to 4 KB page boundaries before allocating — required for page tables.
- **Bitmap PMM** (`pmm_alloc_block` / `pmm_free_block`): tracks every 4 KB physical frame. Initialized by walking the GRUB memory map — only usable (type=1) regions are marked free. Hardware regions like the VGA buffer (`0xA0000`) and BIOS ROM (`0xF0000`) are never handed out.

#### Paging (`paging.c`)
- `init_paging()`: allocates one page directory and 32 page tables via `kmalloc_a`, creating an identity map of the entire first 128 MB of RAM (virtual == physical). Loads the directory into CR3 and sets CR0 bit 31 to enable the MMU.
- `map_page(phys, virt, flags)`: maps a single 4 KB page. Safe to call at any time because the new page table frame allocated by `pmm_alloc_block()` is always within the 128 MB identity map. Calls `invlpg` to flush the TLB for the mapped address.

#### Program Loader (`keyboard.c` — `run` command)
```
run YOOO.BIN
  ↓
pmm_alloc_block()         → allocate one 4 KB physical frame
map_page(phys, 0x40000000, 3)  → map it at virtual 0x40000000
fat16_read_file(..., 0x40000000, 4096)  → load binary into mapped memory
((void(*)())0x40000000)()  → call it as a function
```
The program must be a flat 32-bit binary that ends with `ret`. It runs in ring 0 with full access to hardware.

#### FAT16 Driver (`fat16.c`)
- Reads BPB from sector 0, calculates `fat_start_sector`, `root_dir_start`, `data_start_sector`.
- `fat16_read_file(name8, ext3, buf, max_size)`: follows the FAT cluster chain, clamped to `max_size` to prevent buffer overflow.
- `fat16_write_file`: scans the FAT for free clusters, links the chain, writes data sectors, creates a root directory entry.
- `fat16_format_83`: converts a `"FILE.EXT"` string to the space-padded 8.3 format the FAT directory entry requires.

---

## 🛠️ Build & Run

### Prerequisites

```bash
sudo apt-get install -y nasm gcc grub-pc-bin grub-common xorriso mtools qemu-system-x86
```

### Build

```bash
make
```

The build pipeline:
1. Assembles `*.asm` → `.o` object files
2. Compiles `*.c` → `.o` object files (32-bit freestanding)
3. Links everything with `linker.ld` → `kernel.bin`
4. Assembles `user/YOOO.asm` → `user/YOOO.bin` (flat binary)
5. Injects `YOOO.BIN` into `disk.img` via `mcopy`
6. Packages kernel + initrd into `CIndy-os.iso` via `grub-mkrescue`

### Run

```bash
make run          # QEMU with VGA display
make run-curses   # QEMU with text-only (curses) display
make run-debug    # QEMU headless, kernel output to stdout via port 0xE9
```

---

## 🗺️ Roadmap

### Completed ✅
- [x] GDT — flat 32-bit protected mode
- [x] IDT — 32 CPU exception handlers + hardware IRQs
- [x] PIC remapping (IRQ 0-7 → vectors 32-39)
- [x] PIT — hardware timer at 100 Hz
- [x] PS/2 keyboard driver — scancode-to-ASCII, shift, caps lock
- [x] Interactive shell — `argc/argv` command parser
- [x] Multiboot memory map parsing
- [x] Bump allocator (`kmalloc`, `kmalloc_a`)
- [x] Bitmap Physical Memory Manager — only frees type-1 BIOS regions
- [x] x86 Paging — full identity map of 128 MB, `map_page()` with TLB flush
- [x] USTAR initrd filesystem (`ls`, `cat`)
- [x] ATA PIO disk driver — 28-bit LBA read/write
- [x] FAT16 filesystem — full read/write (`fat-ls`, `fat-cat`, `fat-write`)
- [x] Program Loader — load and execute flat binaries from disk (`run`)

### Future Enhancements 🎯
- [ ] **User Mode (Ring 3)** — separate kernel/user address spaces, privilege separation
- [ ] **System Calls** — `int 0x80` or `syscall` interface
- [ ] **Process Management** — context switching, process table, scheduler
- [ ] **Dynamic Memory Allocator** — `malloc`/`free` with a free-list or buddy allocator
- [ ] **Writable Extended FS** — subdirectories, file deletion
- [ ] **Preemptive Multitasking** — timer-driven task switching
- [ ] **ELF Loader** — load proper ELF executables instead of flat binaries

---

## 🧠 Learning Resources

- [OSDev Wiki](https://wiki.osdev.org/) — comprehensive OS development reference
- [Multiboot Specification](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html)
- [NASM Documentation](https://nasm.us/doc/)
- [James Molloy's Kernel Tutorial](http://www.jamesmolloy.co.uk/tutorial_html/)
- [x86 Assembly Reference](https://www.felixcloutier.com/x86/)
- [USTAR Format](https://www.gnu.org/software/tar/manual/tar.html#SEC141)
- [FAT16 Specification](https://www.win.tue.nl/~aeb/linux/fs/fat/fat-1.html)

---

## 📄 License

Personal learning project. Do whatever you want with it.

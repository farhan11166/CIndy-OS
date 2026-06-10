# 🖥️ CIndy-OS

A bare-metal x86 hobby operating system that boots via GRUB, written from scratch in NASM assembly and C.

---

## 📸 What it does right now

When booted, CIndy-OS:
1. Clears the VGA text-mode screen
2. Prints **"Boot Successful"** on row 0
3. Prints **"Welcome to CIndy-os"** on row 1

All output is written directly to the VGA text buffer at physical address `0xB8000` — no OS, no libc, no drivers.

---

## 🗂️ Project Structure

```
CIndy-os/
├── src/
│   ├── boot.asm       # Assembly entry point + Multiboot header
│   └── kernel.c       # C kernel (VGA print, clear screen)
├── iso/
│   └── boot/
│       ├── kernel.bin # Copied here before ISO creation
│       └── grub/
│           └── grub.cfg   # GRUB menu entry
├── linker.ld          # Linker script – places kernel at 1 MiB
├── Makefile           # Build & run targets
└── CIndy-os.iso       # Bootable ISO output (generated)
```

---

## ⚙️ How It Works

### Boot Flow

```
BIOS → GRUB (reads CIndy-os.iso) → loads kernel.bin
     → finds Multiboot header in .multiboot section
     → jumps to _start (boot.asm)
     → sets up stack (16 KiB)
     → calls kernel_main() (kernel.c)
     → writes text to VGA buffer @ 0xB8000
     → halts (cli + hlt loop)
```

### Key Files Explained

#### `src/boot.asm` — Assembly Entry Point
- Defines the **Multiboot header** (magic `0x1BADB002`, flags, checksum) required by GRUB
- Allocates a 16 KiB stack in `.bss`
- Sets `esp` to the top of the stack
- Calls `kernel_main()` in C
- Enters an infinite `cli` + `hlt` loop if the kernel ever returns

#### `src/kernel.c` — The Kernel
- `clear_screen()` — zeroes out all 80×25 VGA cells
- `print(msg, row)` — writes a string to a specific screen row by directly writing character + attribute bytes to `0xB8000`
- `kernel_main()` — entry point called by boot.asm; clears screen and prints startup messages

#### `linker.ld` — Linker Script
- Sets the kernel load address to **1 MiB** (`0x100000`) — the standard for protected-mode kernels
- Ensures `.multiboot` is the **first section** in the binary (GRUB scans the first 8 KiB)
- Separates `.text`, `.rodata`, `.data`, `.bss` into distinct 4 KiB-aligned segments

#### `iso/boot/grub/grub.cfg` — GRUB Configuration
```
menuentry "Cindy-os" {
    multiboot /boot/kernel.bin
    boot
}
```
Tells GRUB to load `kernel.bin` as a Multiboot kernel.

---

## 🛠️ Build & Run

### Prerequisites

```bash
sudo apt-get install -y nasm gcc grub-pc-bin grub-common xorriso mtools qemu-system-x86
```

| Tool | Purpose |
|------|---------|
| `nasm` | Assemble `boot.asm` → `boot.o` |
| `gcc` | Compile `kernel.c` → `kernel.o` (freestanding, 32-bit) |
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
nasm -f elf32 src/boot.asm -o boot.o
gcc -m32 -ffreestanding -c src/kernel.c -o kernel.o
ld -m elf_i386 -T linker.ld -o kernel.bin boot.o kernel.o
cp kernel.bin iso/boot/kernel.bin
grub-mkrescue -o CIndy-os.iso iso
```

### Run in QEMU

```bash
make run
# equivalent to:
qemu-system-i386 -cdrom CIndy-os.iso
```

---

## 🐛 Bugs Fixed Along the Way

| Bug | Cause | Fix |
|-----|-------|-----|
| `grub-mkrescue: error: 'mformat' invocation failed` | `mtools` package not installed | `sudo apt-get install mtools` |
| `error: no multiboot header found` | `boot.asm` was empty — no Multiboot magic header | Added proper Multiboot header section |
| `parser: instruction expected` on line 5 | `bit 32` typo instead of `bits 32` | Fixed to `bits 32` |
| `missing .note.GNU-stack section` (linker warning) | NASM doesn't emit a GNU stack note by default | Not blocking; can be suppressed by adding `section .note.GNU-stack noalloc noexec nowrite progbits` |
| `LOAD segment with RWX permissions` (linker warning) | Linker script wasn't separating code/data segments | Fixed in `linker.ld` by using separate aligned sections |
| Kernel didn't boot — black screen | `linker.ld` was empty, so `_start` was at wrong address | Wrote proper linker script loading kernel at 1 MiB |

---

## 🗺️ Roadmap / Next Steps

- [x] GDT (Global Descriptor Table) flat setup
- [x] IDT (Interrupt Descriptor Table) + basic interrupt handling
- [x] Programmable Interval Timer (PIT)
- [x] PS/2 keyboard driver
- [x] Simple shell / command input
- [ ] Memory management (PMM, Paging, kmalloc)
- [ ] Basic filesystem (read-only FAT or custom)

---

## 🧠 Learning Resources

- [OSDev Wiki](https://wiki.osdev.org/) — the go-to reference for hobby OS development
- [Multiboot Specification](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html)
- [NASM Documentation](https://nasm.us/doc/)
- [James Molloy's Kernel Tutorial](http://www.jamesmolloy.co.uk/tutorial_html/)

---

## 📄 License

This is a personal learning project. Do whatever you want with it.

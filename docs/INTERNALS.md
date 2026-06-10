# 📖 CIndy-OS — Internals Reference

> A living document explaining how the low-level pieces of CIndy-OS actually work.
> Written so you can come back and revise any concept without searching the internet.

---

## Table of Contents

1. [The Big Picture — What Happens When You Boot](#1-the-big-picture--what-happens-when-you-boot)
2. [The Toolchain — What Each Tool Does](#2-the-toolchain--what-each-tool-does)
3. [Linkers — What They Are and Why You Need One](#3-linkers--what-they-are-and-why-you-need-one)
4. [The Linker Script — `linker.ld` Explained](#4-the-linker-script--linkerld-explained)
5. [The Bootloader — `boot.asm` Explained](#5-the-bootloader--bootasm-explained)
6. [NASM Assembly Concepts Used](#6-nasm-assembly-concepts-used)
7. [The Multiboot Standard](#7-the-multiboot-standard)
8. [The VGA Text Buffer](#8-the-vga-text-buffer)
9. [Ports — I/O in x86](#9-ports--io-in-x86)
10. [The IDT — Interrupt Descriptor Table](#10-the-idt--interrupt-descriptor-table)
11. [The 8259 PIC — Programmable Interrupt Controller](#11-the-8259-pic--programmable-interrupt-controller)
12. [ISR Stubs — The ASM-to-C Bridge](#12-isr-stubs--the-asm-to-c-bridge)
13. [Quick Reference Cheatsheet](#13-quick-reference-cheatsheet)

---

## 1. The Big Picture — What Happens When You Boot

When you power on a machine (or start QEMU), this is the exact sequence of events:

```
Power On
   │
   ▼
BIOS/UEFI firmware runs
   │  (checks hardware, performs POST)
   ▼
BIOS looks for a bootable device
   │  (finds your ISO / CD)
   ▼
BIOS loads GRUB from the first 512 bytes (MBR)
   │
   ▼
GRUB takes over, reads its config
   │  (finds your kernel.bin entry)
   ▼
GRUB scans the first 8 KiB of kernel.bin
   │  looking for the Multiboot magic number 0x1BADB002
   ▼
GRUB switches the CPU to 32-bit Protected Mode
   │  (no more BIOS calls, flat 4 GiB address space)
   ▼
GRUB loads kernel.bin into RAM at 0x100000 (1 MiB)
   │
   ▼
GRUB jumps to your `start` label in boot.asm
   │
   ▼
boot.asm loads CIndy-OS's own GDT and segment registers
   │
   ▼
boot.asm sets the stack pointer
   │
   ▼
boot.asm calls kernel_main() in C
   │
   ▼
Your C kernel runs 🎉
```

**Why 1 MiB?** The first 1 MiB of RAM is reserved for legacy BIOS data, video memory (like the VGA buffer at `0xB8000`), and GRUB itself. Kernels conventionally load at `0x100000` (1 MiB) to stay out of the way.

---

## 2. The Toolchain — What Each Tool Does

| Step | Tool | Command | Output |
|------|------|---------|--------|
| 1 | **NASM** | `nasm -f elf32 src/boot.asm -o boot.o` | ELF32 object file |
| 1 | **NASM** | `nasm -f elf32 src/idt_load.asm -o idt_load.o` | ELF32 object file |
| 1 | **NASM** | `nasm -f elf32 src/isr.asm -o isr.o` | ELF32 object file |
| 1 | **NASM** | `nasm -f elf32 src/interrupts.asm -o interrupts.o` | ELF32 object file |
| 2 | **GCC** | `gcc -m32 -ffreestanding -c src/kernel.c -o kernel.o` | ELF32 object file |
| 2 | **GCC** | `gcc -m32 -ffreestanding -c src/screen.c -o screen.o` | ELF32 object file |
| 2 | **GCC** | `gcc -m32 -ffreestanding -c src/ports.c -o ports.o` | ELF32 object file |
| 2 | **GCC** | `gcc -m32 -ffreestanding -c src/idt.c -o idt.o` | ELF32 object file |
| 2 | **GCC** | `gcc -m32 -ffreestanding -c src/pic.c -o pic.o` | ELF32 object file |
| 3 | **LD** | `ld -m elf_i386 -T linker.ld -o kernel.bin ...` | Final kernel binary |
| 4 | **grub-mkrescue** | `grub-mkrescue -o CIndy-os.iso iso` | Bootable ISO image |
| 5 | **QEMU** | `qemu-system-i386 -cdrom CIndy-os.iso` | Running virtual machine |
| 5 | **QEMU debug** | `make run-debug` | Headless boot log through debug port `0xE9` |

### Key GCC Flags
- `-m32` — compile for 32-bit x86, not 64-bit. Our kernel runs in 32-bit protected mode.
- `-ffreestanding` — tells GCC "there is no C standard library". No `printf`, no `malloc`, no OS. You're the OS.
- `-c` — compile only, do **not** link. Produces a `.o` object file.

---

## 3. Linkers — What They Are and Why You Need One

### The Problem: Object Files Are Incomplete

When you compile a C file with `gcc -c`, you get an **object file** (`.o`). An object file contains compiled machine code, but it is **not a complete program**. It has holes in it — references to functions and variables that live in *other* object files.

For example, `kernel.c` calls `print()`. After compiling `kernel.c`, the resulting `kernel.o` has a placeholder where the address of `print` should go. It doesn't know the address yet — `print` lives in `screen.c`, which is a separate object file.

```
kernel.o        screen.o        ports.o
┌──────────┐    ┌──────────┐    ┌──────────┐
│ kernel   │    │ print()  │    │ outb()   │
│ _main()  │    │ clear_   │    │ inb()    │
│  CALL ?? │    │ screen() │    │          │
└──────────┘    └──────────┘    └──────────┘
     │                │               │
     └────────────────┴───────────────┘
                      │
                   LINKER
                      │
                      ▼
               ┌──────────────┐
               │  kernel.bin  │  ← complete, all addresses resolved
               └──────────────┘
```

### What the Linker Does

The **linker** (`ld`) takes all your `.o` files and:

1. **Merges sections** — all `.text` sections from all object files are combined into one `.text` block, all `.data` into one `.data` block, etc.
2. **Resolves symbols** — replaces every `CALL ??` placeholder with the actual memory address of the target function.
3. **Applies the layout** — uses the linker script to decide exactly where in memory each section lands.
4. **Produces the final binary** — a single `kernel.bin` where every address is concrete.

### The Bug We Hit

```
ld: kernel.o: undefined reference to `outb'
```

This happened because `ports.c` was compiled into `ports.o`, but `ports.o` was **never handed to the linker**. So when the linker tried to resolve `outb` (called from `kernel.c`), it searched all the `.o` files it knew about and found nothing. Fix: add `ports.o` to the `ld` command.

> **Key rule:** Compiling (`gcc -c`) and linking (`ld`) are completely separate steps.
> A file that compiles successfully can still cause a linker error if its output `.o` isn't included in the link step.

---

## 4. The Linker Script — `linker.ld` Explained

The linker script tells `ld` exactly how to arrange the kernel in memory. Without it, `ld` would use its own defaults — and our Multiboot header would end up in the wrong place, causing GRUB to reject the kernel.

```ld
ENTRY(start)
```
> Declares `start` as the **entry point** — the very first instruction that will execute.
> This must match the label name in `boot.asm`. GRUB jumps to this address.

---

```ld
SECTIONS
{
    . = 1M;
```
> The `.` (dot) is the **location counter** — it tracks the current memory address.
> Setting `. = 1M` (= `0x100000`) means "start placing everything at the 1 MiB mark".
> This is the standard load address for protected-mode kernels, safely above legacy BIOS territory.

---

```ld
    .multiboot ALIGN(4K) :
    {
        *(.multiboot)
    }
```
> **This is the most critical section.** GRUB scans the first 8 KiB of the binary for the Multiboot magic.
> By placing `.multiboot` first (and aligning to a 4 KiB boundary), we guarantee it appears before any code.
>
> `*(.multiboot)` means: "collect the `.multiboot` section from **all** object files and put it here."
> In practice only `boot.o` has a `.multiboot` section.
>
> `ALIGN(4K)` — rounds the current address up to the next 4096-byte boundary before placing this section.

---

```ld
    .text ALIGN(4K) :
    {
        *(.text)
    }
```
> **The code section.** All compiled machine instructions from all `.o` files land here.
> `*(.text)` = grab every `.text` section from every object file.

---

```ld
    .rodata ALIGN(4K) :
    {
        *(.rodata)
    }
```
> **Read-only data.** String literals like `"Welcome to CIndy-OS\n"` live here.
> The CPU doesn't enforce read-only at this stage (no paging yet), but it's good practice to separate it.

---

```ld
    .data ALIGN(4K) :
    {
        *(.data)
    }
```
> **Initialised read-write data.** Global variables with an initial value (e.g. `int x = 5;`) go here.

---

```ld
    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss)
    }
```
> **Zero-initialised data.** Global variables with no initial value (e.g. `int x;`) go here.
> The BSS section takes **no space in the binary file** — the OS is responsible for zeroing it at startup.
> `*(COMMON)` handles C's "tentative definitions" (uninitialized globals in C that `gcc` puts in COMMON).

---

### Why ALIGN(4K)?

4 KiB = 4096 bytes = one **page**. Although we don't have paging enabled yet, aligning sections to page boundaries is standard practice. It ensures that when we do enable paging later, each section can get its own page-level permissions (read-only, executable, etc.) without splitting a section across pages.

---

### Memory Map of Our Kernel

```
Address         Section
──────────────────────────────────
0x00000000      [BIOS / reserved]
0x000A0000      [VGA memory region]
0x000B8000      ← VGA text buffer
0x000FFFFF      [BIOS / reserved end]
──────────────────────────────────
0x00100000  ←── kernel starts here (1 MiB)
0x00100000      .multiboot   (Multiboot header)
0x00101000      .text        (machine code)
0x00102000      .rodata      (string literals)
0x00103000      .data        (initialized globals)
0x00104000      .bss         (zero globals + stack)
──────────────────────────────────
```

---

## 5. The Bootloader — `boot.asm` Explained

```asm
global start
extern kernel_main
```
> - `global start` — exports the `start` label so the linker can see it (required for `ENTRY(start)` in linker.ld).
> - `extern kernel_main` — declares that `kernel_main` is defined somewhere else (in `kernel.c`). NASM needs to know this so it can emit a relocation entry for the linker to fill in.

---

```asm
MAGIC     equ 0x1BADB002
FLAGS     equ 0
CHECKSUM  equ -(MAGIC + FLAGS)
CODE_SEG  equ gdt_code - gdt_start
DATA_SEG  equ gdt_data - gdt_start
```
> `equ` defines **compile-time constants** — like `#define` in C but for NASM. No memory is allocated.
>
> - `MAGIC = 0x1BADB002` — the Multiboot magic number. GRUB literally searches byte-by-byte for this value.
> - `FLAGS = 0` — configuration flags for GRUB. `0` means "no special requests" (no memory map, no framebuffer, etc.)
> - `CHECKSUM = -(MAGIC + FLAGS)` — the spec requires that `MAGIC + FLAGS + CHECKSUM == 0` (mod 2³²). This is a basic integrity check so GRUB knows it found a real header, not random data that happens to look like the magic number.
> - `CODE_SEG` and `DATA_SEG` are selector offsets into the kernel's own GDT. With the current layout, code is `0x08` and data is `0x10`.

---

```asm
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
```
> - `section .multiboot` — puts the following data in a section named `.multiboot`. The linker script places this section first.
> - `align 4` — the Multiboot spec requires the header to be 4-byte aligned.
> - `dd` — **Define Doubleword** — emits a 32-bit (4-byte) value. We emit three 4-byte values: the header is 12 bytes total.

---

```asm
section .text
bits 32
```
> - `section .text` — all following code goes into the code section.
> - `bits 32` — tells NASM to emit **32-bit instructions**. Critical: GRUB has already switched the CPU to 32-bit protected mode before jumping to us. If you wrote 16-bit instructions here, the CPU would misinterpret them and crash.

---

```asm
start:
    cli
    lgdt [gdt_descriptor]
    jmp CODE_SEG:flush_segments

flush_segments:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, stack_top
    call kernel_main
```
> `start` is our entry point — the very first instruction GRUB jumps to.
> The kernel installs its own flat GDT before calling C, so it does not depend on GRUB's temporary segment setup.
> The far jump reloads `cs`, then the data segment registers and stack segment are loaded with the kernel data selector.
> `mov esp, stack_top` points the CPU at the 16 KiB stack declared in `boot.asm`.
> `call kernel_main` pushes the return address onto the stack and jumps to `kernel_main()` in C.

---

```asm
hang:
    cli
    hlt
    jmp hang
```
> If `kernel_main` ever returns (it shouldn't), we fall into an **infinite loop** here.
> `hlt` stops the CPU until another interrupt instead of spinning hot forever.
> Without this, the CPU would execute whatever garbage bytes follow in memory — undefined behaviour.
> This is the OS equivalent of `while(true) {}`.

---

## 6. NASM Assembly Concepts Used

| Concept | Syntax | Meaning |
|---------|--------|---------|
| Define constant | `NAME equ VALUE` | Like `#define`. No memory allocated. |
| Define 32-bit value | `dd VALUE` | Emits 4 bytes at current position |
| Define 16-bit value | `dw VALUE` | Emits 2 bytes |
| Define 8-bit value | `db VALUE` | Emits 1 byte |
| Export symbol | `global name` | Makes `name` visible to the linker |
| Import symbol | `extern name` | Tells NASM `name` is defined elsewhere |
| Declare section | `section .name` | Switches to the named section |
| Set instruction width | `bits 32` | Emit 32-bit opcodes (vs 16-bit) |
| Align to boundary | `align N` | Pad to next N-byte boundary |
| Unconditional jump | `jmp label` | Jump to label (no condition) |
| Far jump | `jmp selector:label` | Reload `cs` with a new code segment selector |
| Call function | `call label` | Push return address + jump |
| Load GDT | `lgdt [ptr]` | Load the Global Descriptor Table register |
| Load IDT | `lidt [ptr]` | Load the Interrupt Descriptor Table register |
| Halt CPU | `hlt` | Sleep until the next interrupt |

---

## 7. The Multiboot Standard

Multiboot is a **contract** between your kernel and the bootloader (GRUB). It defines a header format that GRUB looks for to know:
- "Is this actually a kernel I should load?"
- "Where should I load it?"
- "Does it need a memory map? A framebuffer?"

### The Header Format (Multiboot 1)

| Offset | Field | Value in CIndy-OS |
|--------|-------|-------------------|
| 0 | Magic | `0x1BADB002` |
| 4 | Flags | `0x00000000` |
| 8 | Checksum | `-(MAGIC + FLAGS)` |

The spec says: **the header must appear in the first 8192 bytes (8 KiB) of the OS image, aligned to a 4-byte boundary.**

### What GRUB Does After Finding It

1. Loads the entire kernel binary into RAM
2. Switches CPU to 32-bit protected mode
3. Sets up a minimal GDT (Global Descriptor Table)
4. Puts a pointer to a Multiboot info struct in `ebx` (we don't use this yet — needed for Week 7 memory map)
5. Puts `0x2BADB002` in `eax` as a "handshake" value
6. Jumps to the entry point (`start`)

CIndy-OS currently installs its own flat GDT immediately after entry. GRUB gets us into protected mode, but the kernel still takes ownership of segment setup before running C code.

---

## 8. The VGA Text Buffer

The VGA text mode buffer is a block of memory at physical address **`0xB8000`** that maps directly to the screen. Writing bytes here makes characters appear — no OS, no driver needed.

### Layout

The screen is **80 columns × 25 rows = 2000 characters**. Each character takes **2 bytes**:

```
Byte 0: ASCII character code
Byte 1: Attribute byte
         ┌───┬───┬───┬───┬───┬───┬───┬───┐
         │ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │
         └───┴───┴───┴───┴───┴───┴───┴───┘
           │   └───────────┘   └─────────┘
         Blink   Background      Foreground
                  color (3 bits)  color (4 bits)
```

### Address Formula

```c
char *vga = (char *)0xB8000;
int offset = (row * 80 + col) * 2;
vga[offset]     = character;   // ASCII byte
vga[offset + 1] = 0x0F;        // attribute: white on black
```

### Advanced VGA Control

Because we write directly to memory, our kernel handles all terminal logic manually:
- **Colors**: By passing custom attribute bytes (e.g., `0x0A` for light green, `0x0B` for cyan), we implemented a colored prompt and boot banner.
- **Scrolling**: When the cursor reaches the bottom row (row 25), the kernel copies rows 1-24 up to rows 0-23 in memory, and fills row 24 with empty spaces.
- **Hardware Cursor**: The blinking cursor is controlled by VGA I/O ports `0x3D4` and `0x3D5`. GRUB disables it by default, so the kernel un-hides it by setting the scanline registers (`0x0A` and `0x0B`) and continuously updates its position using the high and low position registers (`0x0E` and `0x0F`).

---

## 9. Ports — I/O in x86

x86 has two separate address spaces:
1. **Memory** — accessed with normal load/store (`mov`, pointer dereference)
2. **I/O Ports** — a separate 64 KiB space, accessed with special instructions `in` / `out`

Hardware devices (keyboard, serial port, PIC, PIT, etc.) expose their control registers as I/O ports, not memory addresses.

### Instructions

```asm
; Write 1 byte (AL) to port DX
out %%al, %%dx

; Read 1 byte from port DX into AL
in %%dx, %%al
```

### Our `ports.c` Implementation

```c
void outb(unsigned short port, unsigned char data) {
    __asm__ volatile (
        "out %%al, %%dx"
        :
        : "a" (data), "d" (port)
    );
}
```

- `__asm__ volatile` — inline assembly that the compiler must not optimise away
- `"a" (data)` — put `data` into the `eax` register (the `al` byte is the low byte of `eax`)
- `"d" (port)` — put `port` into the `edx` register (the `dx` word is the low 16 bits)
- `volatile` — tells GCC "this has side effects, do not reorder or eliminate it"

### Key Ports in x86 (for reference)

| Port | Device | Purpose |
|------|--------|---------|
| `0x60` | PS/2 Keyboard | Read scancode / send command |
| `0x64` | PS/2 Controller | Status register / command port |
| `0xE9` | QEMU debug console | Print debug bytes with `-debugcon stdio` |
| `0x20` / `0x21` | PIC (Primary) | 8259 interrupt controller |
| `0xA0` / `0xA1` | PIC (Secondary) | Cascaded PIC |
| `0x40`–`0x43` | PIT | Programmable Interval Timer |
| `0x3F8` | COM1 (Serial) | First serial port |

### QEMU Debug Port `0xE9`

CIndy-OS mirrors `print()` output to port `0xE9`. This port is a QEMU debugging helper, not normal PC hardware, but it is very useful when VGA output or curses display is blank.

```sh
make run-debug
```

That target runs QEMU with `-debugcon stdio -global isa-debugcon.iobase=0xe9`, so every byte written to port `0xE9` appears in the terminal.

---

## 10. The IDT — Interrupt Descriptor Table

The **IDT** tells the CPU where to jump when an interrupt or exception fires. It is an array of 256 **gate descriptors**, each 8 bytes. The CPU finds it via the `IDTR` register, loaded with the `lidt` instruction.

### IDT Entry Structure (8 bytes)

```c
struct idt_entry {
    unsigned short base_low;   // bits 0–15 of handler address
    unsigned short selector;   // code segment selector (0x08 = kernel code)
    unsigned char  always_zero;
    unsigned char  flags;      // gate type, DPL, present bit
    unsigned short base_high;  // bits 16–31 of handler address
} __attribute__((packed));
```

`__attribute__((packed))` tells GCC to not add padding bytes between fields. The CPU expects the exact 8-byte layout defined by the x86 spec.

### IDT Pointer (6 bytes — loaded into IDTR)

```c
struct idt_ptr {
    unsigned short limit;  // size of IDT in bytes - 1  (256 * 8 - 1 = 2047)
    unsigned int   base;   // linear address of the IDT array
} __attribute__((packed));
```

### Gate Flags (`0x8E` for kernel interrupt gates)

```
Bit 7   : Present (must be 1 for a valid gate)
Bits 5-6: DPL — Descriptor Privilege Level (0 = kernel)
Bit 4   : Storage segment (0 for interrupt/trap gates)
Bits 0-3: Gate type (0xE = 32-bit interrupt gate)

0x8E = 1_00_0_1110
       P DPL S  type
```

### How `idt_init()` Works

1. Sets `idtp.limit = sizeof(idt_entry) * 256 - 1`
2. Sets `idtp.base = (uint32_t)&idt` — address of the array
3. Calls `idt_load(&idtp)` — executes `lidt` to install the IDT into the CPU

The keyboard gate is currently registered in `kernel_main()` before interrupts are enabled:

```c
idt_set_gate(33, (unsigned int)isr33, 0x08, 0x8E);
```

This works because `lidt` stores the base address of the `idt` array. If code updates an entry in that same array before `sti`, the CPU sees the updated gate when the interrupt fires.

### Vector Number Layout

| Vectors | Assigned To |
|---------|-------------|
| 0 – 31 | CPU exceptions (#DE, #PF, #GP, …) |
| 32 – 47 | Hardware IRQs 0–15 (after PIC remap) |
| 48 – 255 | Software interrupts / available |

> **IRQ1 (keyboard) → vector 33** because the primary PIC is remapped to start at 32. IRQ0=32, IRQ1=33, …

---

## 11. The 8259 PIC — Programmable Interrupt Controller

The **8259A PIC** sits between hardware devices and the CPU. When a device fires (e.g. keyboard key press), the PIC raises a CPU interrupt at a configured vector number.

### The Problem: Default Mapping Collides with CPU Exceptions

By default the BIOS maps:
- **IRQ 0–7** → CPU vectors **8–15** (same as CPU exceptions `#DF`, `#TS`, `#NP`, `#SS`, `#GP`, `#PF`, `#MF`)

This is catastrophic — a keyboard press would look like a General Protection Fault. **You must remap the PIC before enabling interrupts (`sti`).**

### Remapping to Safe Vectors

```
Primary PIC   (IRQ 0–7)  → vectors 0x20–0x27  (32–39)
Secondary PIC (IRQ 8–15) → vectors 0x28–0x2F  (40–47)
```

### The Initialization Command Word (ICW) Sequence

```c
// ICW1 — start initialization sequence (cascade mode)
outb(0x20, 0x11);   // primary PIC command port
outb(0xA0, 0x11);   // secondary PIC command port

// ICW2 — set vector offsets
outb(0x21, 0x20);   // primary:   IRQ0 → vector 32 (0x20)
outb(0xA1, 0x28);   // secondary: IRQ8 → vector 40 (0x28)

// ICW3 — cascade wiring
outb(0x21, 0x04);   // primary:   IRQ2 is connected to secondary (bit 2 set)
outb(0xA1, 0x02);   // secondary: cascade identity = 2

// ICW4 — environment info
outb(0x21, 0x01);   // 8086/88 mode
outb(0xA1, 0x01);

// OCW1 — mask everything except IRQ1 keyboard
outb(0x21, 0xFD);
outb(0xA1, 0xFF);
```

### PIC Port Map

| Port | PIC | Purpose |
|------|-----|---------|
| `0x20` | Primary | Command |
| `0x21` | Primary | Data / IMR (Interrupt Mask Register) |
| `0xA0` | Secondary | Command |
| `0xA1` | Secondary | Data / IMR |

### End of Interrupt (EOI)

After every hardware IRQ handler runs, you **must** send an EOI to the PIC, otherwise it will never fire that IRQ again:

```c
outb(0x20, 0x20);   // EOI to primary PIC (for IRQ 0–7)
// If IRQ was 8–15, also send to secondary:
outb(0xA0, 0x20);
```

The current `keyboard_handler()` sends EOI after reading port `0x60`, so keyboard interrupts keep firing:

```c
last_scancode = inb(0x60);
key_pressed = 1;
outb(0x20, 0x20);
```

---

## 12. ISR Stubs — The ASM-to-C Bridge

When the CPU fires an interrupt it:
1. Pushes `eflags`, `cs`, `eip` (and sometimes an error code) onto the stack
2. Looks up the handler address in the IDT
3. Jumps to that address in kernel mode

C functions don't know about this — they expect a normal cdecl calling convention. So we need a thin assembly wrapper (the **ISR stub**) to bridge between the CPU's interrupt calling convention and C.

### `isr33` — Keyboard ISR Stub

```asm
global isr33

isr33:
    cli             ; disable interrupts (already off, but defensive)
    pusha           ; push eax, ecx, edx, ebx, esp, ebp, esi, edi

    call keyboard_handler   ; jump to C function

    popa            ; restore all 8 general-purpose registers
    iret            ; return from interrupt (restores eip, cs, eflags)
```

### Why `iret` and Not `ret`?

`ret` just pops `eip` off the stack. But the CPU pushed three things when the interrupt fired: `eflags`, `cs`, and `eip`. Using `ret` would leave the stack misaligned and the CPU would resume at a garbage address. `iret` pops all three in the right order.

### Why `pusha`/`popa`?

The C function `keyboard_handler` can freely modify any register (it follows cdecl). If we don't save registers before calling it, we'd corrupt the register state of whatever code was running when the interrupt fired.

---

## 13. Quick Reference Cheatsheet

```
BOOT SEQUENCE
─────────────────────────────────────────────────────
BIOS → GRUB → (finds 0x1BADB002 in kernel.bin) →
  switches to 32-bit protected mode →
  loads kernel.bin at 0x100000 →
  jumps to start: in boot.asm →
  load kernel GDT + segment registers →
  set esp to stack_top →
  call kernel_main in kernel.c

BUILD PIPELINE
─────────────────────────────────────────────────────
boot.asm      ──[nasm -f elf32]──► boot.o      ─┐
idt_load.asm  ──[nasm -f elf32]──► idt_load.o  ─┤
isr.asm       ──[nasm -f elf32]──► isr.o       ─┤
interrupts.asm──[nasm -f elf32]──► interrupts.o ─┤
kernel.c      ──[gcc -m32 -c]───► kernel.o     ─┤──[ld -T linker.ld]──► kernel.bin
screen.c      ──[gcc -m32 -c]───► screen.o     ─┤
ports.c       ──[gcc -m32 -c]───► ports.o      ─┤        └──[grub-mkrescue]──► CIndy-os.iso
idt.c         ──[gcc -m32 -c]───► idt.o        ─┤
pic.c         ──[gcc -m32 -c]───► pic.o        ─┘

LINKER SCRIPT SECTIONS (in order in memory)
─────────────────────────────────────────────────────
0x100000  .multiboot   ← GRUB magic header (boot.o)
0x101000  .text        ← all machine code
0x102000  .rodata      ← string literals (read-only)
0x103000  .data        ← initialized globals
0x104000  .bss         ← zero globals + stack

IDT GATE FLAGS
─────────────────────────────────────────────────────
0x8E = Present | DPL=0 | 32-bit interrupt gate
Vector 0–31  : CPU exceptions
Vector 32–39 : IRQ 0–7  (primary PIC, remapped)
Vector 40–47 : IRQ 8–15 (secondary PIC, remapped)
Vector 33    : IRQ1 = keyboard

PIC PORTS
─────────────────────────────────────────────────────
0x20 = primary PIC command   0x21 = primary PIC data
0xA0 = secondary PIC command 0xA1 = secondary PIC data
EOI  = outb(0x20, 0x20)  (+ outb(0xA0,0x20) for IRQ 8–15)
Current mask: primary 0xFD (IRQ1 only), secondary 0xFF (all masked)

NASM QUICK REFERENCE
─────────────────────────────────────────────────────
dd VALUE      → 4 bytes (doubleword)
dw VALUE      → 2 bytes (word)
db VALUE      → 1 byte
equ           → constant (no memory)
global        → export symbol to linker
extern        → import symbol from linker
bits 32       → emit 32-bit opcodes
section .name → switch to section
pusha / popa  → save / restore all 8 GPRs
iret          → interrupt return (restores eip, cs, eflags)
sti           → enable maskable interrupts
hlt           → halt until next interrupt

VGA TEXT BUFFER
─────────────────────────────────────────────────────
Base address : 0xB8000
Dimensions   : 80 cols × 25 rows
Bytes/char   : 2 (ASCII + attribute)
Offset formula: (row * 80 + col) * 2
Attribute 0x0F: white text on black background
Attribute 0x6F: white text on brown background
```

---

## 14. Programmable Interval Timer (PIT)

The PIT generates an interrupt (IRQ0 / Interrupt 32) at a programmable frequency.
- **Port `0x43`**: Command port to configure the timer.
- **Port `0x40`**: Data port to set the frequency divisor.
- **Formula**: `1193180 / target_frequency = divisor`.

By ticking a variable every time IRQ0 fires, we can track system uptime and implement sleep functions.

## 15. Keyboard Driver & Shell (`argc/argv` Parser)

When IRQ1 fires (Keyboard), the PIC signals the CPU, which looks up the IDT and calls `isr33`, eventually reaching `keyboard_handler()`.
- The handler reads the scancode from port `0x60`.
- The driver ignores key releases (scancodes with the high bit `0x80` set).
- It maps the scancode to ASCII using a lookup table (`kbd_us`).
- It manages an `input_buffer` to capture user input.
- When `\n` (Enter) is pressed, the shell processes the `input_buffer` by splitting it into an `argc` (argument count) and `argv` (argument vector) array. It replaces spaces with null-terminators (`\0`) to extract individual words.
- These words are compared using a custom `strcmp` function (since we have no `<string.h>`) to dispatch built-in commands like `help`, `echo`, `version`, and `reboot`.

## 16. CPU Exceptions (0-31)

If the kernel performs an illegal operation (e.g., dividing by zero, accessing invalid memory, executing an invalid opcode), the CPU triggers a hardware exception.
- We registered 32 Interrupt Service Routines (`isr0` through `isr31`) in the IDT to catch these.
- Some exceptions push an error code to the stack; for those that don't, our assembly stubs push a dummy `0` to keep the stack frame uniform.
- All 32 stubs jump to `isr_common_stub`, which saves all registers and calls a generic C `fault_handler()`.
- The handler prints the specific exception (e.g., "Page Fault" or "Division By Zero") and halts the CPU. This prevents QEMU from silently rebooting, providing the "Blue Screen of Death" needed to debug kernel panics.

---

*Keep this document updated as CIndy-OS grows. Add a new section each time you touch a new subsystem.*

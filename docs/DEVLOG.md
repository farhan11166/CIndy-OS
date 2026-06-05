# 📓 CIndy-OS — Developer Log

> A bare-metal x86 operating system built from scratch in C and NASM Assembly.
> This log tracks weekly progress, bugs squashed, and things learned along the way.

---

## Week 1 — Booting Into Existence
**Date:** May 2026
**Phase:** Terminal Foundation

### ✅ What I Built
- Set up the entire cross-compilation toolchain (`nasm`, `gcc -m32`, `ld`, `grub-mkrescue`, `qemu`)
- Wrote `boot.asm` — a valid **Multiboot-compliant** bootloader header that GRUB can load
- Set up a **16 KiB stack** in the `.bss` section with proper 16-byte alignment
- Jumped from Assembly into C by calling `kernel_main()`
- Wrote `kernel.c` with:
  - `clear_screen()` — blanks all 80×25 VGA cells by writing spaces to `0xB8000`
  - `print(msg, row)` — writes a string to a specific screen row using direct VGA memory writes
- Configured `linker.ld` to correctly place the multiboot header, stack, and text sections
- Built a `Makefile` pipeline: `nasm → gcc → ld → grub-mkrescue → qemu`
- Successfully booted `CIndy-os.iso` in QEMU and saw:

```
Boot Successful
Welcome to CIndy-os
```

### 🐛 Bugs Fixed
- `bits 32` was written as `bit 32` in boot.asm — NASM silently ignored the invalid directive, causing alignment issues. Fixed.
- `mtools` was missing from the system, which caused `grub-mkrescue` to fail silently. Installed it to unblock the build.

### 📚 What I Learned
- The VGA text buffer lives at physical address `0xB8000`
- Each character cell is **2 bytes**: `[ASCII byte][attribute byte]`
- The attribute byte `0x6F` = white text on brown/yellow background
- Row offset formula: `row × 80 × 2`
- GRUB needs a Multiboot header with a specific magic number (`0x1BADB002`) and valid checksum
- The linker script controls where each section lands in memory — the multiboot header must be in the first 8 KiB of the binary

### 🔧 Tech Stack
| Tool | Purpose |
|---|---|
| NASM | Assemble `boot.asm` to ELF32 |
| GCC (`-m32 -ffreestanding`) | Compile `kernel.c` without stdlib |
| GNU LD | Link with custom `linker.ld` |
| GRUB (`grub-mkrescue`) | Create bootable ISO |
| QEMU | Test the ISO in a virtual machine |

---

## 🔧 Patch — June 1, 2026
**Phase:** Week 1 Cleanup & Bug Fixes

### ✅ What I Did
- **Fixed Makefile indentation bug** — `src/screen.c` compilation line was using spaces instead of a hard tab, causing `make` to fail with `missing separator`. Fixed.
- **Refactored `kernel.c`** — removed inline VGA logic from `kernel.c` and delegated to `screen.c`. `kernel_main` now cleanly calls `clear_screen()`, `print()`.
- **Updated `screen.h`** — renamed `print(str, row)` → `print(str)` and `printAt` → `print_at(str, row, col)` for a cleaner API.
- **Fixed `boot.asm` — "no multiboot header found"** — GRUB was rejecting the kernel because the Multiboot magic signature was missing entirely. Added a proper `.multiboot` section with:
  - `MAGIC = 0x1BADB002`
  - `FLAGS = 0`
  - `CHECKSUM = -(MAGIC + FLAGS)`
- Also noted that `linker.ld` expects entry point `_start` but `boot.asm` declared `start` (no underscore) — to be reconciled when doing the full boot refactor.

### 🐛 Bugs Fixed
| Bug | Root Cause | Fix |
|---|---|---|
| `make`: missing separator | Line indented with spaces not a tab | Replaced with hard tab |
| GRUB: no multiboot header found | `.multiboot` section was absent from `boot.asm` | Added Multiboot header block |

### 📚 What I Learned
- Makefiles are **tab-sensitive** — spaces look identical in most editors but break the parser
- GRUB scans the **first 8 KiB** of the kernel binary for the Multiboot magic `0x1BADB002`
- The Multiboot header must live in its own `.multiboot` section so the linker places it before `.text`
- The checksum is computed as `-(MAGIC + FLAGS)` such that all three 32-bit values sum to zero

---

## 🔧 Patch — June 2, 2026
**Phase:** Week 1 Cleanup & Port Infrastructure

### ✅ What I Did
- **Fixed `undefined reference to 'outb'` linker error** — `src/ports.c` existed and was being compiled but was never passed to the linker. Added `ports.o` to the `ld` command and the corresponding `gcc` compile step to the Makefile.
- **Cleaned up `kernel.c`** — removed throwaway port-testing code (`outb(0x3F8, 1)`, `inb(0x60)`, `print_int(port_val)`) that was left over from experiments. It served no real purpose at this stage and was reading a garbage byte from the keyboard buffer on boot.
- **Explored keyboard LED control** — investigated the `0xED` Set LEDs command for the PS/2 keyboard controller. Learned the correct protocol but deferred implementation: QEMU ignores LED commands, and keyboard I/O belongs in the IRQ1 handler (Week 4), not in `kernel_main`.

### 🐛 Bugs Fixed
| Bug | Root Cause | Fix |
|---|---|---|
| `ld: undefined reference to 'outb'` | `ports.c` was compiled but `ports.o` was never passed to the linker | Added `ports.o` to the `ld` link command |

### 📚 What I Learned
- Compiling a `.c` file (`gcc -c`) and **linking** it are two separate steps — a missing object in the `ld` command causes undefined reference errors even if the source compiled cleanly
- The PS/2 keyboard LED protocol requires: wait for input buffer empty (port `0x64` bit 1) → send `0xED` to `0x60` → wait for ACK (`0xFA`) → send LED bitmask (bit 0 = Scroll Lock, bit 1 = Num Lock, bit 2 = Caps Lock)
- QEMU does not emulate keyboard LEDs — hardware-specific I/O like this must be tested on real hardware

---

## Week 2 — Debug Utilities + IDT + PIC
**Date:** June 6, 2026
**Phase:** ✅ Done

### ✅ What I Built

**Screen Utilities (`screen.c` / `screen.h`)**
- Added **cursor tracking** — `cursor_row` / `cursor_col` global state, updated on every `print()` call
- Added **newline (`\n`) support** inside `print()` — increments `cursor_row`, resets `cursor_col`
- Added **line-wrap** — when `cursor_col >= 80`, automatically advances to the next row
- Added `print_int(int num)` — converts a signed integer to decimal string and prints it via `print()`
- Added `print_hex(unsigned int num)` — prints a 32-bit value as `0xXXXXXXXX` uppercase hex

**IDT (`src/idt.c` + `include/idt.h`)**
- Defined `struct idt_entry` (8 bytes, `__attribute__((packed))`) with fields: `base_low`, `selector`, `always_zero`, `flags`, `base_high`
- Defined `struct idt_ptr` (6 bytes, `__attribute__((packed))`) — the descriptor passed to `lidt`
- Implemented `idt_set_gate(num, base, selector, flags)` — fills one IDT entry
- Implemented `idt_init()` — sets limit/base of the IDT pointer, registers IRQ1 (keyboard) at gate 33 with selector `0x08` and flags `0x8E`, calls `idt_load()`
- Declared `extern void idt_load(struct idt_ptr*)` — implemented in `idt_load.asm` (stub, to be completed)

**ISR (`src/isr.asm`)**
- Wrote `isr33` — the keyboard interrupt stub:
  - `cli` / `pusha` — disable interrupts, save all registers
  - `call keyboard_handler` — jump to C handler (to be implemented in Week 3)
  - `popa` / `sti` / `iret` — restore registers, re-enable interrupts, return from interrupt

**PIC (`src/pic.c` + `include/pic.h`)**
- Implemented `pic_remap()` — remaps the 8259A PIC so hardware IRQs 0–15 no longer clash with CPU exceptions 0–31:
  - Primary PIC: IRQ 0–7 → vectors `0x20–0x27`
  - Secondary PIC: IRQ 8–15 → vectors `0x28–0x2F`
  - Sent init command `0x11`, offsets, cascade wiring, and `0x01` (8086 mode)
  - Unmasked all IRQs on both PICs (`outb(0x21, 0x00)`, `outb(0xA1, 0x00)`)

**Kernel (`kernel.c`)**
- Added includes for `idt.h`, `pic.h`, `ports.h`
- Now calls `print_int(12345)` and `print_hex(0xDEADBEEF)` as debug smoke tests
- Calls `idt_init()` then `pic_remap()` in sequence on boot

**Makefile**
- Added compile steps for `src/idt.c → idt.o`, `src/pic.c → pic.o`
- Added NASM assembly steps for `src/idt_load.asm → idt_load.o`, `src/isr.asm → isr.o`
- All new object files added to the `ld` link command

### 🐛 Bugs / Notes
- `src/idt_load.asm` is currently **empty** — `idt_load` is declared but the `lidt` instruction has not been written yet. This will cause a linker error (`undefined reference to idt_load`) until filled in.
- `keyboard_handler` is called from `isr.asm` but not yet defined in any C file — will be added in Week 3.

### 📚 What I Learned
- The **IDT** has 256 entries, each 8 bytes. CPU exceptions use gates 0–31, hardware IRQs 32–47 (after PIC remap), and the rest are available for software interrupts.
- **Gate flags `0x8E`** = present (`1`) + privilege level 0 (`00`) + storage segment 0 (`0`) + gate type interrupt 32-bit (`1110`)
- The **8259 PIC** defaults map IRQ0–7 to CPU vectors 8–15, which **collide** with CPU exceptions (#DF, #TS, #NP…). You *must* remap before enabling interrupts.
- The PIC init sequence is: ICW1 (start init) → ICW2 (vector offset) → ICW3 (cascade) → ICW4 (mode)
- **ISR stubs** must save/restore all registers (`pusha`/`popa`) and end with `iret`, not `ret` — `iret` restores `cs`, `eip`, and `eflags` that the CPU pushed automatically.
- `idt_load` must call `lidt [idtp]` — the CPU register that holds the IDT descriptor is invisible to C, so it requires one line of assembly.

**New Files Added:**
| File | Purpose |
|---|---|
| `src/idt.c` | IDT entries array, `idt_set_gate`, `idt_init` |
| `include/idt.h` | `idt_entry`, `idt_ptr` structs, function declarations |
| `src/idt_load.asm` | (stub) Will call `lidt` to load the IDT |
| `src/isr.asm` | ISR33 stub — keyboard interrupt handler bridge |
| `src/pic.c` | `pic_remap()` — 8259 PIC initialization |
| `include/pic.h` | `pic_remap()` declaration |

---

## Week 3 — *(Upcoming)*
**Phase:** Keyboard Input & `idt_load` Completion

Complete the interrupt plumbing started in Week 2 and get the first real keyboard input working.

**Planned:**
- [ ] Write `idt_load.asm` — call `lidt [idtp]` to actually install the IDT into the CPU
- [ ] Write `keyboard_handler()` in C — reads scancode from port `0x60`
- [ ] Build a scancode → ASCII lookup table (set 1, US layout)
- [ ] Implement a fixed-size input ring buffer (e.g. 256 chars)
- [ ] Send PIC EOI (`outb(0x20, 0x20)`) at the end of each keyboard ISR
- [ ] Echo typed characters to screen using `print()`
- [ ] Handle `Backspace` (erase last char on screen) and `Enter` (flush buffer)
- [ ] Add ISR stubs for CPU exceptions 0–31 with a generic C fault handler

**New Files:**
| File | Purpose |
|---|---|
| `src/keyboard.c` + `include/keyboard.h` | IRQ1 handler, scancode table, input buffer |

---

## Week 4 — *(Upcoming)*
**Phase:** PIC + Keyboard Input

With the IDT in place, wire up the 8259 PIC and handle IRQ1 (keyboard). This gives CIndy-OS its first interactive input.

**Planned:**
- [ ] Remap the 8259 PIC — offset IRQs to 0x20–0x2F so they don't clash with CPU exceptions
- [ ] Write `pic_send_eoi(uint8_t irq)` — signal End of Interrupt after each IRQ handler
- [ ] Register IRQ1 in the IDT and write the keyboard ISR stub in `isr.asm`
- [ ] Write `keyboard_handler()` in C — reads the scancode from port `0x60`
- [ ] Build a scancode → ASCII lookup table (set 1, US layout)
- [ ] Implement a fixed-size input ring buffer (e.g. 256 chars)
- [ ] Echo typed characters to screen in real time
- [ ] Handle `Backspace` (erase last char) and `Enter` (flush buffer, return string)
- [ ] Test: type characters in QEMU and see them appear on screen

**New Files:**
| File | Purpose |
|---|---|
| `src/pic.c` + `include/pic.h` | PIC init, EOI, enable/disable IRQ |
| `src/keyboard.c` + `include/keyboard.h` | IRQ1 handler, scancode table, input buffer |

---

## Week 5 — *(Upcoming)*
**Phase:** Shell

Build a simple interactive command shell on top of the keyboard input system.

**Planned:**
- [ ] Shell loop: print prompt → read line from keyboard buffer → parse → dispatch → repeat
- [ ] `argc/argv`-style parser: split input on spaces into tokens
- [ ] Implement built-in commands:
  - `help` — list all available commands
  - `clear` — call `clear_screen()`
  - `echo <text>` — print the arguments back
  - `about` — print OS name, version, author
  - `version` — print kernel version string
  - `reboot` — triple-fault or use keyboard controller reset
- [ ] Unknown command → print `"Unknown command: <input>"`
- [ ] Prompt format: `[CIndy-OS]> `

**New Files:**
| File | Purpose |
|---|---|
| `src/shell.c` + `include/shell.h` | Shell loop, command dispatcher, parser |

---

## Week 6 — *(Upcoming)*
**Phase:** Better Terminal + Timers

Polish the terminal experience and add a timer interrupt for time-based features.

**Planned:**
- [ ] Boot splash screen / ASCII banner on startup
- [ ] Colored prompt using `set_color()` (e.g. cyan `[CIndy-OS]>`)
- [ ] Implement screen scrolling — shift all VGA rows up by 1 when cursor hits row 25
- [ ] Set up IRQ0 (timer) via the PIT (Programmable Interval Timer) — ~100 Hz tick
- [ ] Maintain a global `tick_count` and expose `get_uptime_seconds()`
- [ ] Add `uptime` shell command
- [ ] Blinking cursor using timer tick toggling VGA cursor position
- [ ] Optional: status bar at the bottom row showing uptime + OS name

**New Files:**
| File | Purpose |
|---|---|
| `src/timer.c` + `include/timer.h` | PIT setup, IRQ0 handler, tick counter |

---

## Week 7 — *(Upcoming)*
**Phase:** Memory Management

Give the kernel the ability to understand and allocate memory.

**Planned:**
- [ ] Parse the Multiboot info struct passed by GRUB — extract the memory map
- [ ] Print memory regions in `meminfo` using `khex` (base address, length, type)
- [ ] Implement a simple **bump allocator** (`kmalloc(size_t size)`) — linearly advances a pointer through free memory
- [ ] Track total allocated bytes
- [ ] Add `meminfo` shell command — show total RAM, used, free
- [ ] Optional: implement `kfree()` as a no-op stub (for API completeness)

**New Files:**
| File | Purpose |
|---|---|
| `src/memory.c` + `include/memory.h` | Multiboot memory map parser, bump allocator |

---

## Week 8 — *(Upcoming)*
**Phase:** Polish & Portfolio

Make the project presentable and well-documented for a portfolio/GitHub audience.

**Planned:**
- [ ] Reorganize folder structure: `boot/`, `src/`, `include/`, `docs/`
- [ ] Add missing headers (`kernel.h`, `vga.h`, `types.h` with `uint8_t` etc.)
- [ ] Write `LEARNING.md` — one entry per week summarizing what was learned
- [ ] Update `README.md` with a screenshot or GIF of the OS running
- [ ] Record a 60-second QEMU demo video
- [ ] GitHub: add topics (`os-dev`, `x86`, `bare-metal`, `c`, `nasm`), clean description, tidy commit history

---

## 📊 Progress Tracker

| Week | Phase | Status |
|---|---|---|
| 1 | Terminal Foundation | ✅ Done |
| 2 | Debug Utilities + IDT + PIC | ✅ Done |
| 3 | Keyboard Input & IDT completion | 🔜 Next |
| 4 | Shell | ⬜ Planned |
| 5 | Better Terminal + Timers | ⬜ Planned |
| 6 | Memory Management | ⬜ Planned |
| 7 | Polish & Portfolio | ⬜ Planned |

---

*"The best way to understand how computers work is to build one."*


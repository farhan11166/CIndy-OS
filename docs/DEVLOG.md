# 📓 CIndy-OS — Developer Log

> A bare-metal x86 operating system built from scratch in C and NASM Assembly.
> This log tracks weekly progress, bugs squashed, and things learned along the way.
>
> Current plan: keep the kernel small, clear, and understandable while adding the minimum working pieces needed for a stable protected-mode OS.

---

## Patch — August 2026 — Syscall + ELF + Go-Forward Stability

**Phase:** Minimal user-space foundation

### ✅ What I Completed
- replaced the direct VGA write test with a proper syscall-based user program
- added a basic syscall ABI: `exit`, `write`, `read`, `open`, `close`
- tightened pointer validation so it checks actual mapped user pages instead of only trusting a fixed address window
- added minimal ELF detection and a simple PT_LOAD loader for small user programs
- updated project docs so the state of the OS is documented honestly

### 📚 What This Means
This project has moved past "kernel boots, but user mode is unstable" into a more realistic stage:
- user programs can run in ring 3
- a syscall boundary exists
- the kernel can load and dispatch a basic user-space binary
- the next big step is a process struct + cooperative scheduler

### Next milestone
The next project phase should not be a giant rewrite. The next step is small and focused:
1. process structure / PCB
2. a simple round-robin scheduler
3. a proper syscall return convention
4. better fd table behavior
5. then real preemptive multitasking

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
- Confirmed `linker.ld` and `boot.asm` now agree on the entry point name: `start`.

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

## Week 2 — Debug Utilities + IDT + PIC + First Keyboard IRQ
**Date:** June 6-8, 2026
**Phase:** ✅ Done

### ✅ What I Built

**Screen Utilities (`screen.c` / `screen.h`)**
- Added **cursor tracking** — `cursor_row` / `cursor_col` global state, updated on every `print()` call
- Added **newline (`\n`) support** inside `print()` — increments `cursor_row`, resets `cursor_col`
- Added **line-wrap** — when `cursor_col >= 80`, automatically advances to the next row
- Added `print_int(int num)` — converts a signed integer to decimal string and prints it via `print()`
- Added `print_hex(unsigned int num)` — prints a 32-bit value as `0xXXXXXXXX` uppercase hex
- Mirrored `print()` output to QEMU debug port `0xE9` so boot progress can be seen with `make run-debug` even if VGA/curses display is blank

**IDT (`src/idt.c` + `include/idt.h`)**
- Defined `struct idt_entry` (8 bytes, `__attribute__((packed))`) with fields: `base_low`, `selector`, `always_zero`, `flags`, `base_high`
- Defined `struct idt_ptr` (6 bytes, `__attribute__((packed))`) — the descriptor passed to `lidt`
- Implemented `idt_set_gate(num, base, selector, flags)` — fills one IDT entry
- Implemented `idt_init()` — sets limit/base of the IDT pointer and calls `idt_load()`
- Implemented `idt_load.asm` — uses `lidt [idtp]` to install the IDT into the CPU
- Registered IRQ1 (keyboard) at vector 33 with selector `0x08` and flags `0x8E` before enabling interrupts

**ISR (`src/isr.asm`)**
- Wrote `isr33` — the keyboard interrupt stub:
  - `cli` / `pusha` — disable interrupts, save all registers
  - `call keyboard_handler` — jump to the C handler
  - `popa` / `iret` — restore registers and return from interrupt
- Temporarily added a red `K` debug marker in the top-right corner to prove the ISR was firing, then removed it once keyboard interrupts were confirmed

**PIC (`src/pic.c` + `include/pic.h`)**
- Implemented `pic_remap()` — remaps the 8259A PIC so hardware IRQs 0–15 no longer clash with CPU exceptions 0–31:
  - Primary PIC: IRQ 0–7 → vectors `0x20–0x27`
  - Secondary PIC: IRQ 8–15 → vectors `0x28–0x2F`
  - Sent init command `0x11`, offsets, cascade wiring, and `0x01` (8086 mode)
  - Masked every IRQ except IRQ1 so only keyboard interrupts are enabled for now

**Kernel (`kernel.c`)**
- Added includes for `idt.h`, `pic.h`, `ports.h`
- Now calls `print_int(12345)` and `print_hex(0xDEADBEEF)` as debug smoke tests
- Calls `idt_init()`, `pic_remap()`, registers `isr33`, then enables interrupts with `sti`
- Added `keyboard_handler()` in C:
  - reads the raw scancode from port `0x60`
  - stores it in `last_scancode`
  - marks `key_pressed = 1`
  - sends EOI with `outb(0x20, 0x20)`
- Main loop now uses `hlt` and prints raw keyboard scancodes when a key is pressed or released

**Makefile**
- Added compile steps for `src/idt.c → idt.o`, `src/pic.c → pic.o`
- Added NASM assembly steps for `src/idt_load.asm → idt_load.o`, `src/isr.asm → isr.o`, `src/interrupts.asm → interrupts.o`
- All new object files added to the `ld` link command
- Added `run-curses` for terminal-only QEMU display
- Added `run-debug` for headless boot logging through QEMU debug port `0xE9`

### 🐛 Bugs / Notes
- `make run` failed with `gtk initialization failed` in a no-GUI environment. The kernel was not broken; QEMU just could not open a GTK window. Use `make run-debug` or `make run-curses` depending on the environment.
- Linker warnings about executable stack came from NASM objects missing `.note.GNU-stack`. Added the note section to assembly files.
- Boot looked blank in curses/headless mode even though the kernel was running. Mirroring `print()` to QEMU debug port `0xE9` proved the boot path was working.
- Added a small flat GDT in `boot.asm` before calling C so the kernel does not rely on whatever segment setup GRUB happens to leave behind.
- Removed the temporary red `K` ISR debug marker after confirming keyboard interrupts worked.

### 📚 What I Learned
- The **IDT** has 256 entries, each 8 bytes. CPU exceptions use gates 0–31, hardware IRQs 32–47 (after PIC remap), and the rest are available for software interrupts.
- **Gate flags `0x8E`** = present (`1`) + privilege level 0 (`00`) + storage segment 0 (`0`) + gate type interrupt 32-bit (`1110`)
- The **8259 PIC** defaults map IRQ0–7 to CPU vectors 8–15, which **collide** with CPU exceptions (#DF, #TS, #NP…). You *must* remap before enabling interrupts.
- The PIC init sequence is: ICW1 (start init) → ICW2 (vector offset) → ICW3 (cascade) → ICW4 (mode)
- **ISR stubs** must save/restore all registers (`pusha`/`popa`) and end with `iret`, not `ret` — `iret` restores `cs`, `eip`, and `eflags` that the CPU pushed automatically.
- `idt_load` must call `lidt [idtp]` — the CPU register that holds the IDT descriptor is invisible to C, so it requires one line of assembly.
- `sti` enables maskable hardware interrupts globally, but the PIC mask still decides which IRQ lines are allowed through.
- Keyboard press and release are different scancodes. For example, Enter press is `0x1C`, and Enter release is `0x9C`.
- A kernel can be booting correctly even when the display path is blank; debug output through a known I/O port is useful for proving progress.
- GRUB enters protected mode, but the kernel should still set up its own GDT/segments before trusting C code and the stack.

**New Files Added:**
| File | Purpose |
|---|---|
| `src/idt.c` | IDT entries array, `idt_set_gate`, `idt_init` |
| `include/idt.h` | `idt_entry`, `idt_ptr` structs, function declarations |
| `src/idt_load.asm` | Calls `lidt` to load the IDT |
| `src/isr.asm` | ISR33 stub — keyboard interrupt handler bridge |
| `src/pic.c` | `pic_remap()` — 8259 PIC initialization |
| `include/pic.h` | `pic_remap()` declaration |
| `src/interrupts.asm` | `enable_interrupts()` wrapper around `sti` |
| `include/interrupts.h` | `enable_interrupts()` declaration |

---

## 🔧 Patch — June 8, 2026
**Phase:** Boot Reliability, Debugging, and IRQ1 Proof

### ✅ What Changed Today
- Confirmed the kernel was building cleanly with `make`.
- Fixed NASM linker warnings by adding `.note.GNU-stack` metadata to assembly objects.
- Added `run-curses` as a terminal-only QEMU option for environments without GTK.
- Added `run-debug` using QEMU debug port `0xE9`, which made boot progress visible even when VGA/curses looked blank.
- Mirrored `print()` output to debug port `0xE9` from `screen.c`.
- Updated GRUB config to prefer console/text payload mode:
  - `set timeout=0`
  - `set default=0`
  - `terminal_output console`
  - `set gfxpayload=text`
- Added a small flat GDT in `boot.asm` before entering C:
  - kernel code selector: `0x08`
  - kernel data selector: `0x10`
  - reloads `cs` with a far jump
  - reloads `ds`, `es`, `fs`, `gs`, and `ss`
  - sets `esp` to `stack_top`
- Added `enable_interrupts()` in `src/interrupts.asm` as a small wrapper around `sti`.
- Verified IRQ1 keyboard interrupts work:
  - `isr33` runs
  - `keyboard_handler()` reads port `0x60`
  - the handler sends PIC EOI with `outb(0x20, 0x20)`
  - raw scancodes print on screen
- Removed the temporary red `K` debug marker from `isr33` after confirming the interrupt path worked.
- Updated `docs/INTERNALS.md` so the reference matches the current boot, GDT, IDT, PIC, ISR, and debug-console behavior.
- Updated `.gitignore` so generated build artifacts are not treated like source files.

### 🐛 Problems Diagnosed
| Symptom | Actual Cause | Fix / Result |
|---|---|---|
| `make run` failed with `gtk initialization failed` | QEMU was trying to open a GUI window in a no-GUI environment | Added `run-curses` and `run-debug` |
| Boot looked blank in curses/headless mode | The kernel was running, but VGA output was not visible through that display path | Added QEMU debug-console output on port `0xE9` |
| Linker warned about executable stack | NASM objects did not declare `.note.GNU-stack` | Added stack metadata sections |
| Unsure if keyboard ISR was firing | No visible proof from the handler | Temporarily wrote `K` to VGA top-right, then removed it after confirmation |
| Relying on GRUB's segment setup | Kernel entered C without installing its own GDT | Added a simple flat GDT in `boot.asm` |

### ✅ Verification
```sh
make
timeout 6s make run-debug
```

Observed boot log:

```text
Welcome to CIndy-OS
Boot successful
Kernel loaded successfully
Integer test: 12345
Hex test: 0XDEADBEEF
Testing ports
Initializing IDT...
IDT loaded successfully
Remapping PIC...
PIC remapped successfully
Enabling interrupts...
Interrupts enabled.
```

Manual QEMU check also confirmed raw keyboard scancodes appear when keys are pressed and released.

### 📚 What I Learned Today
- A clean build does not always mean a visible boot. Display output can fail while the kernel is actually running.
- Debug ports are extremely useful in OS development because they bypass screen-mode problems.
- The PIC and CPU interrupt flag are two separate gates: `sti` enables interrupts globally, while PIC masks decide which IRQ lines are allowed.
- Keyboard scancodes include both press and release events. Release scancodes usually have the high bit set, such as `0x9C` for Enter release.
- A Multiboot kernel should still take ownership of its own GDT and segment registers before trusting C code.
- Generated files like `.o`, `.bin`, `.iso`, and copied ISO payloads should stay out of normal source commits.

### Next Small Step
- Move keyboard logic out of `kernel.c` into `src/keyboard.c`.
- Filter release scancodes.
- Add a Set 1 scancode-to-ASCII table.
- Start echoing actual characters instead of raw hex values.

---

## Week 3 — 🚧 In Progress
**Phase:** Real Keyboard Input

Raw keyboard interrupts are now working. The next goal is to turn raw scancodes into useful typed input.

**Planned:**
- [x] Write `idt_load.asm` — call `lidt [idtp]` to actually install the IDT into the CPU
- [x] Write `keyboard_handler()` in C — reads scancode from port `0x60`
- [x] Send PIC EOI (`outb(0x20, 0x20)`) at the end of each keyboard ISR
- [x] Print raw scancodes on screen for debugging
- [x] Filter key-release scancodes so key presses are easier to read
- [x] Build a scancode → ASCII lookup table (set 1, US layout)
- [x] Implement a fixed-size input ring buffer (e.g. 256 chars)
- [x] Echo typed characters to screen using `print()`
- [x] Handle `Backspace` (erase last char on screen) and `Enter` (flush buffer)
- [x] Add ISR stubs for CPU exceptions 0–31 with a generic C fault handler

**New Files:**
| File | Purpose |
|---|---|
| `src/keyboard.c` + `include/keyboard.h` | IRQ1 handler, scancode table, input buffer |

---

## Week 4 — ✅ Done
**Phase:** Shell Prep + Cleaner Input

With raw IRQ1 working, clean up keyboard input enough to support a simple shell.

**Planned:**
- [x] Write `pic_send_eoi(uint8_t irq)` — signal End of Interrupt after each IRQ handler
- [x] Move keyboard code from `kernel.c` into `src/keyboard.c`
- [x] Add `include/keyboard.h`
- [x] Echo typed characters to screen in real time using the input buffer
- [x] Handle `Backspace` and `Enter` cleanly
- [x] Test: type characters in QEMU and see them appear on screen

**New Files:**
| File | Purpose |
|---|---|
| `src/keyboard.c` + `include/keyboard.h` | IRQ1 handler, scancode table, input buffer |

---

## Week 5 — ✅ Done
**Phase:** Shell

Build a simple interactive command shell on top of the keyboard input system.

> Stretch after the two-week base: this is useful once raw keyboard input becomes line-based input.

**Planned:**
- [x] Shell loop: print prompt → read line from keyboard buffer → parse → dispatch → repeat
- [x] `argc/argv`-style parser: split input on spaces into tokens
- [x] Implement built-in commands:
  - `help` — list all available commands
  - `clear` — call `clear_screen()`
  - `echo <text>` — print the arguments back
  - `about` — print OS name, version, author
  - `version` — print kernel version string
  - `reboot` — triple-fault or use keyboard controller reset
- [x] Unknown command → print `"Unknown command: <input>"`
- [x] Prompt format: `[CIndy-OS]> `

**New Files:**
| File | Purpose |
|---|---|
| `src/shell.c` + `include/shell.h` | Shell loop, command dispatcher, parser |

---

## Week 6 — ✅ Done
**Phase:** Better Terminal + Timers

Polish the terminal experience and add a timer interrupt for time-based features.

**Planned:**
- [x] Boot splash screen / ASCII banner on startup
- [x] Colored prompt using `set_color()` (e.g. cyan `[CIndy-OS]>`)
- [x] Implement screen scrolling — shift all VGA rows up by 1 when cursor hits row 25
- [x] Set up IRQ0 (timer) via the PIT (Programmable Interval Timer) — ~100 Hz tick
- [x] Maintain a global `tick_count` and expose `get_uptime_seconds()`
- [x] Add `uptime` shell command
- [x] Blinking cursor by enabling the VGA hardware cursor
- [ ] Optional: status bar at the bottom row showing uptime + OS name

**New Files:**
| File | Purpose |
|---|---|
| `src/timer.c` + `include/timer.h` | PIT setup, IRQ0 handler, tick counter |

---

## Week 7 — ✅ Done
**Phase:** Memory Management

Give the kernel the ability to understand and allocate memory.

**Planned:**
- [x] Parse the Multiboot info struct passed by GRUB — extract the memory map
- [x] Print memory regions in `meminfo` using `khex` (base address, length, type)
- [x] Implement a simple **bump allocator** (`kmalloc(size_t size)`) — linearly advances a pointer through free memory
- [x] Track total allocated bytes
- [x] Add `meminfo` shell command — show total RAM, used, free
- [x] Optional: implement `kfree()` as a no-op stub (for API completeness)

**New Files:**
| File | Purpose |
|---|---|
| `src/memory.c` + `include/memory.h` | Multiboot memory map parser, bump allocator |

---

## Week 8 — ✅ Done
**Phase:** File System (Initrd & USTAR)

Give the kernel the ability to read files loaded by GRUB.

**Planned:**
- [x] Configure GRUB to load an `initrd.tar` archive alongside the kernel.
- [x] Extract `mods_addr` from the Multiboot Information structure.
- [x] Write a USTAR parser to iterate through 512-byte headers.
- [x] Implement the `ls` shell command to list archive contents.
- [x] Implement the `cat` shell command to read raw file data.

**New Files:**
| File | Purpose |
|---|---|
| `src/fs.c` + `include/fs.h` | TAR header definition and USTAR parser logic. |
| `fs/` directory | Directory packaged into the `initrd.tar` archive during build. |

---

## Week 9 — 🚧 In Progress
**Phase:** Polish & Portfolio

Make the project presentable and well-documented for a portfolio/GitHub audience.

**Planned:**
- [x] Add missing standard headers (`types.h` with `uint8_t` etc.)
- [x] Write `LEARNING.md` — summarizing low-level concepts mastered.
- [x] Update `README.md` with the new file system feature.
- [x] Update `INTERNALS.md` with the file system parsing logic.
- [ ] Record a 60-second QEMU demo video/GIF.
- [ ] GitHub: clean description, tidy commit history.

---

## Week 10 — ✅ Done
**Phase:** ATA PIO Hard Drive Driver

Built a bare-metal disk driver that reads and writes 512-byte sectors directly over the ATA PIO bus.

**What I Built:**
- Added `inw` and `outw` (16-bit port I/O) to `ports.c` and `ports.h`.
- Created `src/ata.c` and `include/ata.h` implementing:
  - `ata_wait_ready()` — polls the alternate status register (`0x3F6`) to wait for BSY to clear.
  - `ata_read_sector(lba, buf)` — sends LBA28 command `0x20`, reads 256 words via `inw(0x1F0)`.
  - `ata_write_sector(lba, buf)` — sends command `0x30`, writes 256 words, then flushes cache with `0xE7`.
- Added `write-test` and `read-test` shell commands to verify persistence.
- Updated `Makefile` to compile `ata.c` and added `disk.img` to QEMU's `-drive` flag.
- Added Shift key and Caps Lock support to `keyboard.c` via a second lookup table `kbd_us_shift`.

**New Files:** `src/ata.c`, `include/ata.h`

---

## Week 11 — ✅ Done
**Phase:** Virtual Memory (Paging)

Enabled the x86 MMU (Memory Management Unit) to translate virtual addresses into physical ones.

**What I Built:**
- Added `kmalloc_a()` to `memory.c` — a page-aligned allocator that bumps `free_mem_addr` to the next 4 KB boundary before allocating.
- Created `src/paging.c` and `include/paging.h` implementing `init_paging()`:
  - Allocates a 1024-entry Page Directory and a 1024-entry Page Table using `kmalloc_a`.
  - Identity-maps the first 4 MB of RAM: `page_table[i] = (i * 0x1000) | 3`.
  - Loads `page_direc` into `CR3` and sets bit 31 of `CR0` to enable paging.
- Verified with an intentional Page Fault at `0xA0000000` — Exception 14 was correctly caught by our ISR.

**New Files:** `src/paging.c`, `include/paging.h`

---

## Week 12 — ✅ Done
**Phase:** FAT16 Filesystem Driver

Building a real read/write filesystem parser on top of the ATA driver.

**What I Built:**
- Created `src/fat16.c` and `include/fat16.h` handling the complete FAT16 implementation:
  - `fat16_bpb_t` — a packed struct mirroring the FAT16 Boot Sector / BIOS Parameter Block.
  - `fat16_dir_entry_t` — a packed 32-byte struct for root directory entries.
  - Implemented core calculations: FAT offset, cluster-to-LBA, and traversing the FAT linked list.
- Implemented full **File Write** support:
  - `fat16_find_free_cluster` to allocate empty blocks from the FAT table.
  - Chunked cluster writing using `ata_write_sector`.
  - Creating new 32-byte directory entries in the root directory.
- Implemented full **File Read** support following the FAT chains until `0xFFFF`.
- Wrote string standard library functions (`strncmp`, `memset`, `memcpy`) in `string.c` needed for freestanding kernels.
- Exposed the driver to the OS shell via new keyboard commands:
  - `fat-ls` — View all files
  - `fat-write <file.ext> <text>` — Allocate clusters, link chains, and write to disk!
  - `fat-cat <file.ext>` — Follow FAT chains to read files back!

**New Files:** `src/fat16.c`, `include/fat16.h`

## 📊 Progress Tracker

| Week | Phase | Status |
|---|---|---|
| 1 | Terminal Foundation | ✅ Done |
| 2 | Debug Utilities + IDT + PIC + First Keyboard IRQ | ✅ Done |
| 3 | Real Keyboard Input | ✅ Done |
| 4 | Shell Prep + Cleaner Input | ✅ Done |
| 5 | Shell | ✅ Done |
| 6 | Better Terminal + Timers | ✅ Done |
| 7 | Memory Management | ✅ Done |
| 8 | File System (Initrd) | ✅ Done |
| 9 | ATA PIO Hard Drive Driver | ✅ Done |
| 10 | Virtual Memory (Paging) | ✅ Done |
| 11 | FAT16 Filesystem Driver | ✅ Done |

---

*"The best way to understand how computers work is to build one."*

---

## Week 13 — ✅ Done
**Phase:** Program Loader (Exec)

Built a true program loader so CIndy-OS can read a flat binary from the FAT16 disk, allocate and map memory for it, and execute it.

**What I Built:**
- Added `pmm_alloc_block()` and `pmm_free_block()` to `memory.c` — a bitmap physical memory manager. `mem_bitmap[8192]` (32KB) tracks every 4KB frame of RAM as a single bit.
- `init_pmm(struct multiboot_info* mbd)` walks the GRUB memory map and only marks type-1 (usable) regions as free. Hardware-reserved regions (VGA buffer `0xA0000`, BIOS ROM `0xF0000`) stay permanently marked as used.
- Added `map_page(phys, virt, flags)` to `paging.c`. Finds the correct PDE/PTE, allocates a new page table from the PMM if needed, writes the mapping, and calls `invlpg` to flush the TLB.
- Extended `init_paging()` to identity-map all 128 MB of RAM (32 page tables × 4 MB each) instead of only the first 4 MB.
- Added the `run <file.ext>` shell command in `keyboard.c`:
  - Calls `pmm_alloc_block()` → allocates one 4 KB physical frame.
  - Calls `map_page(phys, 0x40000000, 3)` → maps it at virtual `0x40000000`.
  - Calls `fat16_read_file(..., (uint8_t*)0x40000000, 4096)` → loads the binary straight into mapped memory.
  - Casts `0x40000000` to a function pointer and calls it.
- Created `user/YOOO.asm` — a flat 32-bit binary that writes `'X'` with bright red on white (`0x4F58`) to the bottom-right VGA cell (`0xB8F9E`) and returns with `ret`.
- Updated `Makefile` to automatically assemble `user/YOOO.asm → user/YOOO.bin` with `nasm -f bin` and inject it into `disk.img` with `mcopy` as a build dependency.

**Bugs Fixed During This Phase:**
| Bug | Root Cause | Fix |
|---|---|---|
| `run` crashed or did nothing | `init_pmm` was never called — `max_frames = 0`, so `pmm_alloc_block` returned 0 (physical address 0 = IVT) | Added `init_pmm(mbd)` call in `kernel.c` after `init_memory` |
| Binary not loaded into mapped memory | `fat16_read_file` was called with a local stack buffer `buf[4096]`, not `0x40000000` | Changed call to pass `(uint8_t*)0x40000000` directly |
| `fat16_format_83` stopped parsing at `'0'` | `!= '0'` (character zero) instead of `!= '\0'` (null terminator) | Fixed typo |
| `X` not visible on screen | Program wrote to row 0 col 79 (top-right) which was overwritten by boot banner | Changed target to row 24 col 79 (bottom-right, `0xB8F9E`) |

**New Files:**
| File | Purpose |
|---|---|
| `user/YOOO.asm` | Test flat binary: writes bright red `X` to bottom-right VGA corner and returns |

---

## Week 14 — ✅ Done
**Phase:** Bug Fixes & Hardening

Audited the entire codebase and fixed the highest-priority bugs.

**What I Fixed:**

**PMM hardware region safety (`memory.c`)**
- Rewrote `init_pmm` to take `struct multiboot_info*` instead of a raw `uint32_t` total.
- Now walks the GRUB mmap and only clears (frees) frames in type-1 regions. Everything else stays marked used by default — including the VGA buffer (`0xA0000`), BIOS ROM (`0xE0000`+), and frame 0 (IVT/BDA). Previously `pmm_alloc_block` could return `0xA0000` and corrupt the screen.

**Full 128 MB identity map (`paging.c`)**
- `init_paging` now allocates 32 page tables (was 1) covering all 128 MB of RAM.
- Before this fix, `map_page` used a physical address returned by `pmm_alloc_block` as a virtual pointer to write into the new page table. Once the kernel grew past 4 MB, that pointer would be outside the identity-mapped range and cause a page fault or silent memory corruption. Now physical == virtual for all of RAM, so this is always safe.
- Added `invlpg` TLB flush after every new PTE write.

**`fat16_read_file` buffer overflow (`fat16.c`, `fat16.h`)**
- Added a `max_size` parameter. The read loop now clamps `bytes_left = min(file_size, max_size)` before copying.
- Updated all call sites: `fat-cat` passes `sizeof(buf)` (4096), `run` passes 4096.

**`isr_common_stub` double ds restore (`isr.asm`)**
- Fixed duplicate `mov ds, ax` — the second line was supposed to be `mov ss, ax`. `ss` was never restored after being overwritten to `0x10`.

**Progress Tracker Update:**

| Week | Phase | Status |
|---|---|---|
| 1 | Terminal Foundation | ✅ Done |
| 2 | Debug Utilities + IDT + PIC + First Keyboard IRQ | ✅ Done |
| 3 | Real Keyboard Input | ✅ Done |
| 4 | Shell Prep + Cleaner Input | ✅ Done |
| 5 | Shell | ✅ Done |
| 6 | Better Terminal + Timers | ✅ Done |
| 7 | Memory Management | ✅ Done |
| 8 | File System (Initrd) | ✅ Done |
| 9 | ATA PIO Hard Drive Driver | ✅ Done |
| 10 | Virtual Memory (Paging) | ✅ Done |
| 11 | FAT16 Filesystem Driver | ✅ Done |
| 12 | Program Loader (Exec) | ✅ Done |
| 13 | Bug Fixes & Hardening | ✅ Done |

---

## Patch — 2026-08-31 — PMM Frame Aliasing Fix

**Phase:** User-mode stability

### Problem
`run YOOO.BIN` always page-faulted at `0x40001000` in ring 3.  The binary never
executed a single real instruction.

### Root Cause Found
`kmalloc_a` and `pmm_alloc_block` were not in sync.  The PMM bitmap only knew about
allocations up to the `free_mem_addr` value when `init_pmm` ran at boot.  Every
`kmalloc_a` call after boot produced a "ghost" allocation invisible to the PMM.
The file-read buffer for the binary and the user page frame ended up at the same
physical address.  `memset` of the mapped user page zeroed the source buffer before
`memcpy` could read it, so the user page was filled with zeros.

### Fix
- Added `pmm_mark_used(phys, size)` and hooked it into `kmalloc_a` so bump and bitmap
  allocators stay permanently in sync.
- Fixed `memcpy`-before-`memset` ordering in the flat binary load path.

### Lesson
Two allocators sharing physical memory must share the same tracking state.  The
bump allocator was a fast kernel-init tool that was never designed to coexist with the
PMM at runtime.  The fix makes `kmalloc_a` a thin wrapper that also registers with the
PMM, keeping the kernel's two allocation paths consistent.

See `docs/BUGREPORT_PMM_FRAME_ALIAS.md` for full analysis.

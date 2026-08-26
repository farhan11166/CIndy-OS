# What I Learned Building CIndy-OS

Building an operating system from scratch is notoriously difficult because you have no standard library, no memory manager, and no safety net. If a single pointer is wrong, the CPU halts. 

This project forced me to understand exactly how software interacts with hardware at the lowest possible level. Here are the core concepts I mastered while building CIndy-OS:

## 1. Memory-Mapped I/O (VGA Text Buffer)
Before you can even print "Hello World", you need a screen driver. I learned how to directly manipulate physical memory (`0xB8000`) to draw characters to the screen, manage cursor coordinates (`0x3D4` and `0x3D5` hardware ports), and handle manual screen scrolling in C.

## 2. Bootstrapping and Protected Mode
I learned how the BIOS loads the GRUB bootloader, and how GRUB transitions the CPU from 16-bit Real Mode into 32-bit Protected Mode. I wrote a `boot.asm` stub to define the Multiboot header, set up the stack, and load a flat Global Descriptor Table (GDT) before jumping into the C kernel.

## 3. Interrupts and Hardware Exceptions
To prevent the OS from silently rebooting on errors (like a Divide by Zero), I built an Interrupt Descriptor Table (IDT). I learned how to:
- Map 32 specific CPU exceptions to Assembly stubs.
- Push error codes to the stack.
- Route them to a generic C `fault_handler()` to create a "Blue Screen of Death" for debugging.

## 4. Port I/O and PIC Remapping
To get keyboard input, I had to communicate with the Programmable Interrupt Controller (PIC). I learned how to use `inb` and `outb` port I/O to remap IRQs so they didn't collide with CPU exceptions, enabling the OS to safely receive hardware interrupts from the timer and keyboard.

## 5. Drivers without a Standard Library
Without `<stdio.h>` or `<string.h>`, I wrote my own PS/2 keyboard driver. It listens to port `0x60`, ignores key releases via bitmasks, maps scancodes to ASCII using a lookup table, and feeds an `input_buffer` to power an interactive `argc/argv` command shell.

## 6. Dynamic Memory Allocation
I learned how the OS discovers physical RAM. By parsing the GRUB Multiboot Information structure, I extracted the memory map to find usable RAM regions. Since writing a full `malloc()` is incredibly complex, I implemented a fast **Bump Allocator** (`kmalloc`) that aligns memory to 4-byte boundaries and securely manages pointer bumps above the kernel block.

## 7. Initial Ramdisks and File Systems
To bootstrap a filesystem, I learned how Linux uses an `initrd`. I configured GRUB to load a `TAR` archive into physical memory, and wrote a USTAR parser in C to jump through memory in 512-byte blocks. This allowed my OS to support real file reading (`ls`, `cat`) immediately upon boot.

## 8. ATA PIO Hard Drive Driver
I eventually went beyond the RAM disk and implemented a true ATA PIO hard drive driver! I learned how to communicate with the primary IDE bus using 16-bit port I/O (`inw`, `outw`). I implemented 28-bit LBA (Logical Block Addressing) to target specific sectors, and mastered polling hardware status registers (checking the `BSY` and `DRQ` bits) to synchronize the CPU with the physical disk controller for raw sector read/write operations.

## 9. FAT16 File System and Struct Mapping
I expanded on the ATA driver by formatting the disk with a FAT16 filesystem. I learned how to read the FAT16 Boot Sector (BIOS Parameter Block) and how to perfectly map C structures over raw memory buffers using `__attribute__((packed))`. By casting a `uint8_t` array pointer to a struct pointer, I realized that C doesn't copy data, but simply places a "stencil" over memory to read variables exactly as they are laid out on the disk.

I also learned the FAT16 **8.3 filename format**: names are exactly 8 bytes and extensions exactly 3 bytes, stored space-padded (not null-terminated). Because they lack the `\0` terminator, standard C `strcmp` behaves unpredictably and reads past the end of the buffer. This forced me to implement a custom `strncmp` in my freestanding kernel to compare exactly 8 or 3 bytes. I also had to implement my own `memset` and `memcpy` since OS kernels don't have access to `<string.h>`.

Finally, I mastered **File Allocation Table (FAT) cluster chains**. To read a file, I calculate its starting LBA, read its data, and then read the 16-bit FAT entry for that cluster to find the next one, stopping when `0xFFFF` is encountered. To write a file, I do the reverse: I scan the FAT table for free `0x0000` slots, chain them together, copy the raw data blocks into the corresponding LBAs, and finally overwrite a `0xE5` (deleted) or `0x00` (empty) root directory entry to link the filename to the start of the chain.

## 10. Hardware State & BIOS Handoff
I discovered a fascinating bug regarding hardware state persisting from the boot process. When booting from a CD-ROM in QEMU, the BIOS leaves the CD-ROM as the "selected" IDE drive. Polling for the hard drive's "Ready" status (`0x40`) before explicitly switching the IDE controller to the hard drive resulted in an infinite hang, because CD-ROMs respond differently to status checks.

## 11. Virtual Memory & Paging
Paging was the most complex but most rewarding concept I implemented. I built and enabled the x86 MMU from scratch. I learned that every memory access the CPU makes passes through a two-level table — a Page Directory pointing to Page Tables, each entry mapping a 4 KB "page" of virtual memory to a physical address. I built an **identity map** for the first 4 MB, loaded my Page Directory into register `CR3`, and flipped bit 31 of `CR0` to activate the MMU. I proved it worked by deliberately triggering a **Page Fault (Exception 14)** which my ISR correctly caught. Paging is the foundational layer that makes memory protection, user mode, and multi-processing possible.

---

*This project demystified the "magic" of operating systems. I now have a deep, practical understanding of pointers, memory management, and hardware interaction.*


## 12. Bitmap Physical Memory Manager

A bitmap PMM is the standard first step beyond a bump allocator. I learned that tracking physical frames with one bit each is efficient — 8192 `uint32_t` words cover 1 GB of addressable RAM using only 32 KB of memory.

The critical design insight: **start with all bits set (everything used), then only free regions the GRUB memory map explicitly says are safe.** This ensures hardware-mapped regions (VGA buffer, BIOS ROM) are permanently off-limits without needing to explicitly reserve them.

I also learned the difference between the bump allocator and the PMM: the bump allocator is one-way and used only at boot time for permanent structures. The PMM is bidirectional (alloc + free) and used for dynamic runtime allocations like page tables for new mappings.

## 13. Identity Mapping and the Physical-as-Virtual Pointer Problem

This was the subtlest bug I fixed. When paging is enabled, every pointer the CPU dereferences is a **virtual address**. This means:

```c
uint32_t phys = pmm_alloc_block();     // e.g. 0x500000
uint32_t* ptr = (uint32_t*)phys;       // treated as VIRTUAL 0x500000
ptr[0] = something;                    // page fault if 0x500000 not mapped!
```

The only safe way to use a physical address as a pointer after enabling paging is if that address is inside an identity-mapped region (virtual == physical). So the solution is: **identity-map all of RAM before enabling paging**, not just the first 4 MB you know the kernel uses. Then any frame the PMM returns is guaranteed to be accessible as a pointer.

32 page tables × 4 MB = 128 MB covered. Cost: 128 KB of RAM. Completely worth it.

## 14. The TLB and `invlpg`

The Translation Lookaside Buffer (TLB) is a cache inside the CPU that stores recently used virtual→physical translations. When you write a new PTE into a page table, the CPU doesn't automatically invalidate the old cached translation for that address.

`invlpg` flushes exactly one TLB entry:
```asm
invlpg [virtual_address]
```

Without it, you can have a valid PTE in the page table but the CPU still uses the old (stale) cached translation — so your new mapping appears to not work, or worse, points at the wrong physical address. This is a subtle class of bug that only manifests in specific timing or reuse scenarios.

## 15. Flat Binary Program Execution

An ELF file has headers, sections, relocations — lots of infrastructure. A flat binary is just raw machine code bytes starting at offset 0. When you assemble with `nasm -f bin`, that's exactly what you get.

To execute a flat binary loaded at address `0x40000000`:
```c
void (*fn)() = (void(*)())0x40000000;
fn();
```

The C compiler emits a `call` instruction, which pushes a return address then jumps to the target. The binary just needs to end with `ret` to pop that return address and come back to the kernel cleanly.

The key constraint is that the binary must use **absolute addresses** for any memory it accesses (like VGA at `0xB8F9E`), not relative ones, because there's no linker script or relocation table telling it where it was loaded.

---

*This project demystified the "magic" of operating systems. I now have a deep, practical understanding of pointers, memory management, and hardware interaction.*

---

# Deep dives (concise)

Below are short, focused explanations for the four subsystems you asked for: PMM, GDT/TSS, ATA PIO, and Paging. Each one gives the core concept, why it matters, and what to check when it breaks.

## Bitmap Physical Memory Manager (PMM)

What it is:
- The PMM tracks physical 4 KiB frames using a bitmap: one bit per frame (0=free, 1=used).
- The kernel initializes the bitmap from the GRUB memory map: mark everything used, then mark ranges of type=1 as free.

Why it matters:
- Any subsystem that needs a physical frame (page table, user page, disk buffer) calls `pmm_alloc_block()` to get a frame.
- If PMM is wrong, `pmm_alloc_block()` can return invalid frames (0 or reserved regions), causing corruption or crashes.

Key operations:
- allocate: find a zero bit, set it to 1, return frame_base = index * 4096
- free: clear the bit
- helpers: mark_range(addr, length, used/free)

Common bugs & checks:
- PMM not initialized → all frames treated as used (or all free) → allocations fail or return invalid 0
- Incorrect GRUB memory parsing → important hardware areas accidentally freed (VGA, BIOS ROM)
- Off-by-one when converting bytes ↔ frames → wrong range marked

How to debug:
- Print `max_frames` and the first few bitmap words during boot.
- Verify `pmm_alloc_block()` never returns 0 after `init_pmm()` completes (unless exhausted).
- Temporarily reserve known addresses (VGA at 0xB8000) and ensure allocator never returns them.

## GDT & TSS (Global Descriptor Table and Task State Segment)

What it is:
- GDT: an array of descriptors that define code/data segments and special descriptors (like TSS).
- TSS: a structure the CPU can use to load a stack when switching privilege levels (ring 3 → ring 0).

Why it matters:
- The GDT establishes the CPU's segment selectors (CS/DS). Even in a flat 32-bit model we still need correct descriptors.
- The TSS contains `esp0`/`ss0` used by the CPU when an interrupt or an `iret` forces a privilege change back to ring 0; a malformed TSS descriptor or wrong `esp0` causes immediate faults on privilege transitions.

Key facts:
- A 32-bit (available) TSS descriptor uses a 16-byte entry in the GDT (two consecutive 8-byte slots). Its type field is 0x9 (available) or 0xB (busy) with the present bit set.
- `esp0` must point to a valid kernel stack page. The kernel should update it before entering user mode for each process/thread.

Common bugs & checks:
- TSS descriptor only 8 bytes or with wrong flags → CPU misreads TSS → page faults on ring change.
- `esp0` points into unmapped memory → page fault on hardware-initiated ring change.
- Forgetting to allocate two GDT entries (8 * 2 = 16 bytes) for a 32-bit TSS descriptor.

How to debug:
- Print GDT descriptor bytes after `write_tss_descriptor()` and verify the type (0x89) and flags include D/B where needed.
- Confirm `tss.esp0` value before iret/jump to user mode.
- Validate GDT layout in `boot.asm` matches the selectors used across the codebase (0x08, 0x10, 0x1B, 0x23, 0x28).

## ATA PIO (Programmed I/O) driver

What it is:
- ATA PIO is the classic way to read/write sectors from a disk using I/O ports (no DMA).
- Steps: select drive, write LBA high bits to head register, set sector count and LBA low/mid/high, issue READ/WRITE command, poll status bits, read/write data via `inw`/`outw`.

Why it matters:
- The FAT16 driver and the `run` loader depend on correct sector reads and writes.
- Mistakes in drive selection or polling can hang the kernel or return corrupted data.

Key registers & bits (primary IDE):
- Data: 0x1F0 (read/write words)
- Error / Features: 0x1F1
- Sector Count: 0x1F2
- LBA Low/Mid/High: 0x1F3/0x1F4/0x1F5
- Drive/Head: 0x1F6 (also contains LBA top bits and drive select)
- Status: 0x1F7 (BSY=0x80, DRQ=0x08, ERR=0x01)
- Command: 0x1F7 (write a command like 0x20 for READ)

Common bugs & checks:
- Not selecting the correct drive before polling (CD-ROM vs HDD) → infinite wait
- Polling loop that never checks for ERR or timeout → hang
- Using `inb`/`inw` mismatch (byte vs word) → corrupted sector contents

How to debug:
- Log the status port `0x1F7` and print BSY/DRQ/ERR while issuing commands.
- Ensure `outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F))` is called before waiting for ready.
- Add a timeout to the polling loops and print diagnostic if timeout hits.

## Paging & TLB (quick practical guide)

What it is:
- Paging translates virtual addresses to physical addresses using a page directory and page tables (two-level on x86 without PAE).
- CR3 holds the physical base of the page directory; CR0.PG enables paging.
- TLB caches translations; `invlpg` invalidates a single virtual address translation.

Why it matters:
- Paging lets the kernel isolate processes, protect kernel memory, and present each process its own virtual layout.
- A wrong PTE/PDE or forgetting `invlpg` is the fastest way to get mysterious page faults.

Key practical rules:
- Identity-map a region large enough that kernel code which uses physical addresses (e.g., PMM frames during early boot) can safely dereference returned physical addresses.
- When creating a new page table: allocate a physical frame, write the PDE with present bit, then fill the page table entries, then `invlpg` the mapped virtual addresses as needed.
- Always set the present bit when writing a PTE; set R/W and user bits according to access level.

Common bugs & checks:
- Using a physical address as a pointer after paging is enabled when that physical address isn't identity-mapped → page fault
- Forgetting to set the present bit in PDE/PTE → access fault
- Not calling `invlpg` after replacing a mapping in a running context where the old translation is cached

How to debug:
- Print `cr0`/`cr3` values during boot.
- Print PDE/PTE contents for the virtual address you expect to be mapped.
- Use the page-fault handler to print CR2 (faulting virtual address) and the error code to know if a read/write/user issue occurred.

---

# Next practical steps (short list)

1. Add unit-style tests in QEMU for the `run` flow: script boot → `run YOOO.BIN` → validate return. This will prevent regressions.
2. Add timeouts and diagnostics to ATA polling loops to avoid hangs on faulty devices.
3. Add `panic()` helper that prints state (CR2, EIP, registers) and drops to a safe hlt loop — use in PMM and paging assertion failures.
4. Replace the flat binary loader with a minimal ELF loader so user programs can be compiled with normal toolchains.
5. Implement a simple syscall dispatcher with a guarded trampoline (validate arguments) rather than special-case exit handling.

If you'd like, I can implement (1) the QEMU automated test script and (2) the ATA polling timeouts next — pick one and I’ll start.  

If you want extra detail on any single point above (e.g., a walkthrough of how to compute PDE/PTE indices and flags, or a step-by-step ATA READ sector example with status checks), say which one and I’ll add a compact code-level walkthrough.
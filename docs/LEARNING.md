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

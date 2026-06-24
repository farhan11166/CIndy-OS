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

---

*This project demystified the "magic" of operating systems. I now have a deep, practical understanding of pointers, memory management, and hardware interaction.*

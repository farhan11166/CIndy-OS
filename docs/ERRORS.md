# 🐛 CIndy-OS — Error Log & Debugging

A record of interesting bugs, hangs, and panics encountered during development, along with their root causes and solutions.

## Current debugging focus

At the current stage, the main issue is not a crashy kernel boot path anymore. The critical work is now architectural and focused on correctness, not just basic survival.

The current known areas to watch are:
- syscall return values and validation of user pointers
- per-process memory ownership
- file descriptor semantics for `open/read/write/close`
- scheduler correctness once multiple processes exist
- ELF runtime assumptions for real compiled user binaries

The project is in a healthy place for this stage: the low-level machine bugs are mostly fixed, and the remaining work is more about OS design discipline than platform debug pain.

---

## 1. The CD-ROM IDE Polling Hang (FAT16 Implementation)

**Symptoms:** 
When running `make run`, QEMU booted from the CD-ROM, initialized GRUB, loaded the kernel, but then completely froze. No output, no panic. Just an infinite hang requiring a forceful kill (Ctrl+C).

**Code Involved:**
```c
void ata_read_sector(unsigned int lba, unsigned char* buffer){
    ata_wait_ready(); // Hung here!
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F)); // Select drive
    // ...
}

void ata_wait_ready(){
    while (inb(0x1F7) & 0x80){}
    while (!(inb(0x1F7) & 0x40)){} // Infinite loop!
}
```

**Root Cause:**
When booting from a Hard Drive, the BIOS automatically selects Drive 0 (Master) as the active IDE device. Our previous code relied on this assumption. However, when we added `-boot d` to QEMU to force booting from the CD-ROM, the BIOS left the CD-ROM (an ATAPI device) as the actively selected IDE drive. 
When `ata_wait_ready()` polled the status port `0x1F7` for the `0x40` (Ready) bit, it was querying the CD-ROM, not the hard drive. ATAPI CD-ROMs do not always assert the Ready bit in the same way, causing an infinite loop.

**Solution:**
The IDE Controller must be told which drive to talk to *before* waiting for it to be ready. We swapped the order of operations in `ata_read_sector` and `ata_write_sector`:
```c
void ata_read_sector(unsigned int lba, unsigned char* buffer){
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F)); // 1. Select the Hard Drive FIRST
    ata_wait_ready();                         // 2. NOW wait for it to be ready
    // ...
}
```

## 2. Booting a Data Disk by Mistake

**Symptoms:**
QEMU booted to SeaBIOS and immediately failed with: `This is not a bootable disk. Please insert a bootable floppy and press any key to try again ...`

**Root Cause:**
We ran `mkfs.fat -F 16 disk.img` to format our raw disk image as FAT16. This command wrote a standard FAT16 Boot Sector (BPB) to LBA 0 of the disk. A standard BPB ends with the magic boot signature bytes `0x55 0xAA`. 
Because QEMU's default boot priority tried the Hard Drive first, the BIOS saw `0x55 0xAA` at Sector 0 and assumed it was a bootable OS. It executed the tiny bootloader provided by `mkfs.fat`, which simply prints the error message.

**Solution:**
We updated the QEMU command in the `Makefile` to include `-boot d`, explicitly telling QEMU to prioritize the CD-ROM (`CIndy-os.iso`) which contains GRUB.

---

## 3. `run` Command Does Nothing / Triple Fault

**Symptoms:**
Typing `run YOOO.BIN` printed "Loaded 10 bytes, jumping to 0x40000000..." but either nothing happened, the OS froze, or QEMU triple-faulted and rebooted.

**Root Causes (three separate bugs):**

**A) `init_pmm` was never called**
`kernel.c` called `init_memory` and `init_paging` but not `init_pmm`. With `init_pmm` never called, `max_frames = 0`. Every call to `pmm_alloc_block()` immediately hit `frame >= max_frames` and returned `0`. Physical address `0` is the real-mode Interrupt Vector Table. The `run` command was mapping the IVT to `0x40000000` and jumping into it — a guaranteed crash.

**Fix:** Added `init_pmm(mbd)` in `kernel.c` between `init_memory` and `init_paging`.

**B) Binary loaded into a stack buffer, not the mapped address**
Even after fixing A, the binary was read into `uint8_t buf[4096]` on the kernel stack, not into `0x40000000`. The `program()` call was jumping into whatever uninitialized bytes happened to be at `0x40000000`.

**Fix:** Changed the `fat16_read_file` call to pass `(uint8_t*)0x40000000` directly.

**C) `fat16_format_83` had `!= '0'` instead of `!= '\0'`**
The filename parsing loop had a typo — it stopped at the character `'0'` (ASCII 48) instead of the null terminator `'\0'` (ASCII 0). For `YOOO.BIN` this happened not to matter, but for any filename containing the digit `0` it would produce a wrong 8.3 name and `fat16_read_file` would return "file not found".

**Fix:** Changed `!= '0'` to `!= '\0'`.

---

## 4. Program Executes But `X` Is Not Visible

**Symptoms:**
`run YOOO.BIN` completed successfully ("Program returned."), but no `X` appeared on screen.

**Root Cause:**
`YOOO.asm` was originally writing to `0xB8000 + 158` = row 0, col 79 (top-right corner). The ASCII art boot banner fills row 0 on startup, overwriting that cell. Then every `scroll_screen()` call shifts all rows up, moving the `X` further up until it scrolls off the top.

Additionally, the original binary on disk was assembled without `[BITS 32]` at some point, resulting in a `0x66` operand-size prefix in the instruction stream, though the address encoding was still correct.

**Fix:**
Changed the write target to row 24, col 79 (bottom-right corner): `0xB8000 + (24*80 + 79)*2 = 0xB8F9E`. Changed the color to `0x4F` (bright red on white) for maximum visibility. Also added the `user/YOOO.bin` build rule and `mcopy` injection step to the Makefile so the binary is always rebuilt and updated in `disk.img` automatically.

---

## 5. `map_page` Causes Page Fault After Kernel Grows Past 4 MB

**Symptoms:**
Does not crash immediately, but any call to `map_page` for a virtual address outside the first 4 MB range causes a page fault. Specifically, when `pmm_alloc_block()` returns a physical address ≥ `0x400000`, writing through that pointer with paging enabled faults.

**Root Cause:**
`init_paging` only created one page table covering the first 4 MB. `map_page` allocates a new page table via `pmm_alloc_block()` and then does:
```c
uint32_t* page_table = (uint32_t*)new_table_phy; // physical addr used as virtual!
page_table[i] = 0;
```
With paging on, the CPU translates `new_table_phy` as a virtual address. Any physical address ≥ `0x400000` has no virtual mapping → page fault.

**Fix:**
Extended `init_paging` to create 32 page tables (128 MB identity map). Now every physical address the PMM could ever return has `virtual == physical`, making the pointer always safe to dereference.

---

## 6. Ring-3 Crash: Invalid TSS Descriptor and Stale Stack on Exit

**Symptoms:**
The kernel boots, prints the startup banner, loads the `run` command, and then trips a page fault immediately after jumping to user mode or after `int 0x80` returns.

**Root Cause:**
There were two separate but related bugs:

1. The GDT TSS descriptor was not a valid 16-byte 32-bit TSS descriptor. That meant the CPU could not trust the TSS when switching to ring 3.
2. The user-mode exit path restored the kernel stack and then still executed `iret` on stale state, leaving the CPU to resume with the wrong stack frame.

**Fix:**
- Expanded the TSS slot in the GDT to a full 16-byte descriptor.
- Saved the original kernel `esp` before entering ring 3.
- Restored that saved kernel stack before returning to the kernel call site instead of reusing the user stack frame.

This is the final fix that made the user-mode transition path stable instead of crashing on the first ring switch.

---

## 7. Page Fault at `0x40001000` After `run YOOO.BIN` (PMM / kmalloc_a Frame Aliasing)

**Symptoms:**
`run YOOO.BIN` loaded the binary, jumped to ring 3, and immediately faulted:
```
EXCEPTION: Page Fault
  EIP: 0X40001000   CS: 0X0000001B
  CR2 (fault addr): 0X40001000
  Cause: not present, read, user mode
```
Only one page was ever mapped for the binary (`0x40000000`), so a fault at the second
page (`0x40001000`) looked impossible — the binary is only 53 bytes.

**Root Cause:**
`kmalloc_a` uses a bump allocator.  `init_pmm` marks frames used only up to
`free_mem_addr` *at boot time*.  Any `kmalloc_a` call that happens later (at runtime,
e.g. inside the `run` command) advances `free_mem_addr` without telling the PMM bitmap.
`pmm_alloc_block()` therefore treats those frames as free and hands them out again.

In the `run` command this produced the following collision:
- `kmalloc_a(4096)` for `tmp_buf` took frame F.
- YOOO.bin was read into `tmp_buf` at frame F.
- `pmm_alloc_block()` for the user page also returned frame F (PMM thought it was free).
- `map_page` mapped vaddr `0x40000000` → phys F.
- `memset((void*)0x40000000, 0, 4096)` zeroed phys F — destroying `tmp_buf`.
- `memcpy((void*)0x40000000, img, 53)` copied 53 zero bytes — no valid code.
- The CPU entered ring 3, executed `0x00 0x00` bytes, and crashed.
- The fault appeared at `0x40001000` specifically because the TLB had cached the
  mapping for page 0 (`invlpg` at `map_page` time), so page 0 was accessible, but
  page 1 caused a TLB miss → page-table walk → PTE not present → fault.

**Fix:**
1. Added `pmm_mark_used(phys, size)` in `src/memory.c` / `include/memory.h`.
2. `kmalloc_a` now calls `pmm_mark_used` on every allocation, keeping the PMM bitmap
   permanently in sync with the bump allocator.
3. Changed the flat binary load order in `keyboard.c`: `memcpy` the binary first, then
   `memset` only the tail, so the source is never destroyed before the copy finishes.

Full analysis: `docs/BUGREPORT_PMM_FRAME_ALIAS.md`

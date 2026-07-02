# 🐛 CIndy-OS — Error Log & Debugging

A record of interesting bugs, hangs, and panics encountered during development, along with their root causes and solutions.

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

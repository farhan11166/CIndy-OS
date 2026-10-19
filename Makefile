DISK_IMG := disk.img

.PHONY: all clean run run-curses run-debug

all: $(DISK_IMG) user/YOOO.bin user/hello_elf.elf
	nasm -f elf32 src/boot.asm -o boot.o
	nasm -f elf32 src/idt_load.asm -o idt_load.o
	nasm -f elf32 src/isr.asm -o isr.o
	nasm -f elf32 src/interrupts.asm -o interrupts.o
	nasm -f elf32 src/gdt_flush.asm -o gdt_flush.o
	nasm -f elf32 src/usermode.asm -o usermode.o

	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/kernel.c -o kernel.o
	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/screen.c -o screen.o
	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/ports.c -o ports.o
	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/idt.c -o idt.o
	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/pic.c -o pic.o
	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/keyboard.c -o keyboard.o
	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/timer.c -o timer.o
	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/string.c -o string.o
	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/memory.c -o memory.o
	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/fs.c -o fs.o
	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/ata.c -o ata.o
	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/fat16.c -o fat16.o
	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/paging.c -o paging.o
	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/gdt.c -o gdt.o
	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/syscall.c -o syscall.o
	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/process.c -o process.o
	gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c src/scheduler.c -o scheduler.o

	ld -m elf_i386 -T linker.ld -o kernel.bin \
    boot.o kernel.o screen.o ports.o idt.o pic.o isr.o idt_load.o interrupts.o \
    keyboard.o timer.o string.o memory.o fs.o ata.o fat16.o paging.o gdt.o \
    syscall.o process.o scheduler.o gdt_flush.o usermode.o

	cp kernel.bin iso/boot/kernel.bin
	cd fs && tar -cvf ../initrd.tar *
	cp initrd.tar iso/boot/initrd.tar

	grub-mkrescue -o CIndy-os.iso iso

clean:
	rm -f *.o *.bin *.iso initrd.tar
	rm -f user/*.bin user/*.elf user/*.o
	rm -f iso/boot/kernel.bin iso/boot/initrd.tar
$(DISK_IMG):
	@rm -f $@
	@truncate -s 20M $@
	@mkfs.fat -F 16 $@

user/YOOO.bin: user/YOOO.asm $(DISK_IMG)
	nasm -f bin user/YOOO.asm -o user/YOOO.bin
	mcopy -i $(DISK_IMG) -o user/YOOO.bin ::YOOO.BIN

user/hello_elf.elf: user/hello_elf.asm $(DISK_IMG)
	nasm -f elf32 user/hello_elf.asm -o user/hello_elf.o
	ld -m elf_i386 -N -o user/hello_elf.elf user/hello_elf.o
	mcopy -i $(DISK_IMG) -o user/hello_elf.elf ::HELLO.ELF

run: $(DISK_IMG)
	qemu-system-i386 -boot d -cdrom CIndy-os.iso -drive file=$(DISK_IMG),format=raw,index=0,media=disk

run-curses: $(DISK_IMG)
	qemu-system-i386 -boot d -display curses -cdrom CIndy-os.iso -drive file=$(DISK_IMG),format=raw,index=0,media=disk

run-debug: $(DISK_IMG)
	qemu-system-i386 -boot d -display none -debugcon stdio -global isa-debugcon.iobase=0xe9 -cdrom CIndy-os.iso -drive file=$(DISK_IMG),format=raw,index=0,media=disk

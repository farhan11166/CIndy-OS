all:
	nasm -f elf32 src/boot.asm -o boot.o
	nasm -f elf32 src/idt_load.asm -o idt_load.o
	nasm -f elf32 src/isr.asm -o isr.o
	nasm -f elf32 src/interrupts.asm -o interrupts.o

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

	ld -m elf_i386 -T linker.ld -o kernel.bin boot.o kernel.o screen.o ports.o idt.o pic.o isr.o idt_load.o interrupts.o keyboard.o timer.o string.o memory.o fs.o ata.o

	cp kernel.bin iso/boot/kernel.bin
	cd fs && tar -cvf ../initrd.tar *
			cp initrd.tar iso/boot/initrd.tar
	

	


	grub-mkrescue -o CIndy-os.iso iso

run:
	qemu-system-i386 -cdrom CIndy-os.iso -drive file=disk.img,format=raw,index=0,media=disk

run-curses:
	qemu-system-i386 -display curses -cdrom CIndy-os.iso -drive file=disk.img,format=raw,index=0,media=disk

run-debug:
	qemu-system-i386 -display none -debugcon stdio -global isa-debugcon.iobase=0xe9 -cdrom CIndy-os.iso -drive file=disk.img,format=raw,index=0,media=disk

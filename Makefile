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
	

	ld -m elf_i386 -T linker.ld -o kernel.bin boot.o kernel.o screen.o ports.o idt.o pic.o isr.o idt_load.o interrupts.o

	cp kernel.bin iso/boot/kernel.bin

	grub-mkrescue -o CIndy-os.iso iso

run:
	qemu-system-i386 -cdrom CIndy-os.iso

run-curses:
	qemu-system-i386 -display curses -cdrom CIndy-os.iso

run-debug:
	qemu-system-i386 -display none -debugcon stdio -global isa-debugcon.iobase=0xe9 -cdrom CIndy-os.iso

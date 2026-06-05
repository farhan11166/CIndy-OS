all:
	nasm -f elf32 src/boot.asm -o boot.o
	nasm -f elf32 src/idt_load.asm -o idt_load.o
	nasm -f elf32 src/isr.asm -o isr.o

	gcc -m32 -ffreestanding -c src/kernel.c -o kernel.o
	gcc -m32 -ffreestanding -c src/screen.c -o screen.o
	gcc -m32 -ffreestanding -c src/ports.c -o ports.o
	gcc -m32 -ffreestanding -c src/idt.c -o idt.o
	gcc -m32 -ffreestanding -c src/pic.c -o pic.o
	

	ld -m elf_i386 -T linker.ld -o kernel.bin boot.o kernel.o screen.o ports.o idt.o pic.o isr.o

	cp kernel.bin iso/boot/kernel.bin

	grub-mkrescue -o CIndy-os.iso iso

run:
	qemu-system-i386 -cdrom CIndy-os.iso
all:
	nasm -f elf32 src/boot.asm -o boot.o

	gcc -m32 -ffreestanding -c src/kernel.c -o kernel.o
	gcc -m32 -ffreestanding -c src/screen.c -o screen.o

	ld -m elf_i386 -T linker.ld -o kernel.bin boot.o kernel.o screen.o

	cp kernel.bin iso/boot/kernel.bin

	grub-mkrescue -o CIndy-os.iso iso

run:
	qemu-system-i386 -cdrom CIndy-os.iso
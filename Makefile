CFLAGS=-m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector
LDFLAGS=-m elf_i386 -T link.ld

SOURCES=multiboot_header.asm kernel_entry.asm kernel.c disk.c string.c graphics.c
OBJS=multiboot_header.o kernel_entry.o kernel.o disk.o string.o graphics.o

all: kernel.elf os.iso

multiboot_header.o: multiboot_header.asm
	nasm -f elf multiboot_header.asm -o multiboot_header.o

kernel_entry.o: kernel_entry.asm
	nasm -f elf kernel_entry.asm -o kernel_entry.o

kernel.o: kernel.c
	gcc $(CFLAGS) -c kernel.c -o kernel.o

graphics.o: graphics.c
	gcc $(CFLAGS) -c graphics.c -o graphics.o

kernel.elf: $(OBJS) link.ld
	ld $(LDFLAGS) $(OBJS) -o kernel.elf

os.iso: kernel.elf grub/grub.cfg
	mkdir -p isodir/boot/grub
	cp kernel.elf isodir/boot/kernel.elf
	cp grub/grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o os.iso isodir

clean:
	rm -rf *.o *.elf isodir os.iso

run:
	qemu-system-x86_64 -cdrom os.iso

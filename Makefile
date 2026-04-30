CC   = clang
LD   = ld.lld
NASM = nasm

CFLAGS = -target x86_64-elf -ffreestanding -fno-stack-protector -fno-pic \
         -m64 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mcmodel=kernel \
         -O2 -Wall -Wextra -Ikernel -Ilimine

SRCS = kernel/main.c \
       kernel/arch/x86_64/gdt.c \
       kernel/arch/x86_64/idt.c \
       kernel/arch/x86_64/pic.c \
       kernel/arch/x86_64/pit.c \
       kernel/mm/pmm.c \
       kernel/mm/vmm.c \
       kernel/mm/heap.c \
       kernel/drivers/video/fb.c \
       kernel/drivers/video/terminal.c \
       kernel/drivers/input/keyboard.c \
       kernel/sched/scheduler.c \
       kernel/fs/vfs.c \
       kernel/fs/ramfs.c \
       kernel/user/syscall.c \
       kernel/user/elf.c \
       kernel/user/userspace.c \
       kernel/shell/shell.c \
       kernel/lib/printf.c

OBJS = $(SRCS:.c=.o) \
       kernel/arch/x86_64/entry.o \
       kernel/arch/x86_64/gdt_asm.o \
       kernel/arch/x86_64/isr.o \
       kernel/sched/context_switch.o \
       kernel/user/syscall_asm.o \
       kernel/user/userspace_asm.o

KERNEL = kernel.elf

all: iso

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(NASM) -f elf64 $< -o $@

$(KERNEL): $(OBJS)
	$(LD) -T kernel/arch/x86_64/linker.ld -nostdlib -m elf_x86_64 -o $@ $(OBJS)

iso: $(KERNEL)
	rm -rf iso
	mkdir -p iso/boot
	cp $(KERNEL) iso/boot/kernel.elf
	cp limine/limine-bios.sys iso/boot/
	cp limine/limine-bios-cd.bin iso/boot/
	cp limine/limine-uefi-cd.bin iso/boot/
	cp limine/BOOTX64.EFI iso/boot/
	cp limine.conf iso/limine.conf
	xorriso -as mkisofs \
		-b boot/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		-graft-points \
		boot/=iso/boot \
		limine.conf=iso/limine.conf \
		-o nexusos.iso

run: iso
	qemu-system-x86_64 -cdrom nexusos.iso

clean:
	rm -rf $(OBJS) $(KERNEL) nexusos.iso iso/
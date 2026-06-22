CC = gcc
AS = nasm
LD = ld

INC  = -Ikernel -Ikernel/core -Ikernel/ui -Ikernel/arch -Ikernel/lib \
       -Ikernel/fs -Ikernel/mm -Ikernel/proc -Ikernel/net \
       -Ikernel/drivers -Ikernel/drivers/block -Ikernel/drivers/bus \
       -Ikernel/drivers/char -Ikernel/drivers/input -Ikernel/drivers/net \
       -Ikernel/drivers/timer -Ikernel/drivers/block -Ikernel/ipc
CFLAGS = -std=gnu99 -ffreestanding -m32 -O2 -Wall -Wextra -nostdlib \
         -fno-stack-protector -mno-sse -mno-mmx -mno-sse2 -mno-3dnow $(INC)
LDFLAGS = -m elf_i386 -T link.ld -nostdlib
ASFLAGS = -f elf32

BUILD = build

OBJS = $(BUILD)/boot.o             \
       $(BUILD)/main.o             \
       $(BUILD)/printk.o           \
       $(BUILD)/screen.o           \
       $(BUILD)/fb.o               \
       $(BUILD)/editor.o           \
       $(BUILD)/font.o             \
       $(BUILD)/keyboard.o         \
       $(BUILD)/mouse.o            \
       $(BUILD)/ports.o            \
       $(BUILD)/string.o           \
        $(BUILD)/cpuid.o            \
       $(BUILD)/fat32.o            \
       $(BUILD)/rtc.o              \
       $(BUILD)/isr.o              \
       $(BUILD)/idt.o              \
       $(BUILD)/isr_asm.o          \
       $(BUILD)/page.o             \
       $(BUILD)/paging.o           \
       $(BUILD)/heap.o             \
       $(BUILD)/slab.o             \
       $(BUILD)/task.o             \
       $(BUILD)/sched.o            \
       $(BUILD)/pid.o              \
       $(BUILD)/irq.o              \
       $(BUILD)/signal.o           \
       $(BUILD)/pit.o              \
        $(BUILD)/ata.o              \
        $(BUILD)/blockdev.o         \
       $(BUILD)/syscall.o          \
       $(BUILD)/pci.o              \
       $(BUILD)/partition.o        \
       $(BUILD)/serial.o           \
       $(BUILD)/cmos.o             \
       $(BUILD)/pcspkr.o           \
       $(BUILD)/dma.o              \
        $(BUILD)/floppy.o           \
        $(BUILD)/elf.o              \
       $(BUILD)/ext2.o             \
       $(BUILD)/ntfs.o             \
       $(BUILD)/btrfs.o            \
       $(BUILD)/zfs.o              \
       $(BUILD)/rtl8139.o          \
       $(BUILD)/init.o             \
       $(BUILD)/tty.o              \
       $(BUILD)/initd.o            \
        $(BUILD)/shm.o              \
        $(BUILD)/msg.o              \
        $(BUILD)/netif.o            \
        $(BUILD)/net_utils.o        \
        $(BUILD)/arp.o              \
        $(BUILD)/ip.o               \
        $(BUILD)/icmp.o             \
        $(BUILD)/udp.o              \
        $(BUILD)/tcp.o              \
        $(BUILD)/socket.o           \
        $(BUILD)/gdt.o              \
        $(BUILD)/hw.o               \
        $(BUILD)/vm.o               \
        $(BUILD)/ahci.o             \
        $(BUILD)/blockcache.o       \
        $(BUILD)/vsprintf.o        \
        $(BUILD)/initcall.o        \
        $(BUILD)/wait.o            \
        $(BUILD)/workqueue.o       \
        $(BUILD)/timer.o           \
        $(BUILD)/completion.o      \
        $(BUILD)/idr.o             \
        $(BUILD)/ctype.o           \
        $(BUILD)/rbtree.o          \
        $(BUILD)/script.o

all: $(BUILD)/noctua.bin

$(BUILD)/noctua.bin: $(OBJS) link.ld | $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/boot.o: boot/boot.asm | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/main.o: kernel/core/main.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/screen.o: kernel/ui/screen.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/fb.o: kernel/ui/fb.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/editor.o: kernel/ui/editor.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/font.o: kernel/ui/font.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/keyboard.o: kernel/drivers/input/keyboard.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/mouse.o: kernel/drivers/input/mouse.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/ports.o: kernel/arch/ports.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/string.o: kernel/lib/string.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/printk.o: kernel/lib/printk.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/cpuid.o: kernel/arch/cpuid.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/fat32.o: kernel/fs/fat32.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/rtc.o: kernel/drivers/timer/rtc.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/isr_asm.o: kernel/arch/isr.asm | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/isr.o: kernel/arch/isr.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/idt.o: kernel/arch/idt.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/page.o: kernel/mm/page.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/paging.o: kernel/mm/paging.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/heap.o: kernel/mm/heap.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/slab.o: kernel/mm/slab.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/pid.o: kernel/proc/pid.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/irq.o: kernel/arch/irq.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/task.o: kernel/proc/task.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/sched.o: kernel/proc/sched.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/signal.o: kernel/proc/signal.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/pit.o: kernel/drivers/timer/pit.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/ata.o: kernel/drivers/block/ata.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/blockdev.o: kernel/drivers/block/blockdev.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/syscall.o: kernel/core/syscall.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/pci.o: kernel/drivers/bus/pci.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/partition.o: kernel/drivers/block/partition.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/serial.o: kernel/drivers/char/serial.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/cmos.o: kernel/drivers/timer/cmos.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/pcspkr.o: kernel/drivers/char/pcspkr.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/dma.o: kernel/drivers/block/dma.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

        $(BUILD)/floppy.o: kernel/drivers/block/floppy.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/elf.o: kernel/core/elf.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/ext2.o: kernel/fs/ext2.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/ntfs.o: kernel/fs/ntfs.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/btrfs.o: kernel/fs/btrfs.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/zfs.o: kernel/fs/zfs.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/rtl8139.o: kernel/drivers/net/rtl8139.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/init.o: kernel/core/init.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/tty.o: kernel/core/tty.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/initd.o: kernel/core/initd.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/shm.o: kernel/ipc/shm.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/msg.o: kernel/ipc/msg.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/netif.o: kernel/net/netif.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/net_utils.o: kernel/net/net_utils.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/arp.o: kernel/net/arp.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/ip.o: kernel/net/ip.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/icmp.o: kernel/net/icmp.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/udp.o: kernel/net/udp.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/tcp.o: kernel/net/tcp.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/socket.o: kernel/net/socket.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

iso: $(BUILD)/noctua.bin
	mkdir -p iso/boot
	cp $(BUILD)/noctua.bin iso/boot/noctua.bin
	grub-mkrescue -o noctua.iso iso

run: iso
	qemu-system-i386 \
		-cdrom noctua.iso \
		-cpu max \
		-device piix3-usb-uhci \
		-device usb-tablet \
		-device isa-debug-exit,iobase=0xF4

run-hdd: iso hdd.img
	qemu-system-i386 \
		-hda hdd.img \
		-cdrom noctua.iso \
		-cpu max \
		-device piix3-usb-uhci \
		-device usb-tablet
		
run-serial: iso
	qemu-system-i386 -cdrom noctua.iso -serial stdio

run-serial-hdd: iso hdd.img
	qemu-system-i386 -hda hdd.img -cdrom noctua.iso -serial stdio

run-kernel: $(BUILD)/noctua.bin
	qemu-system-i386 -kernel $(BUILD)/noctua.bin

$(BUILD)/gdt.o: kernel/arch/gdt.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/hw.o: kernel/arch/hw.asm | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/vm.o: kernel/mm/vm.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/ahci.o: kernel/drivers/block/ahci.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/blockcache.o: kernel/fs/blockcache.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/script.o: kernel/core/script.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/ctype.o: kernel/lib/ctype.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/rbtree.o: kernel/lib/rbtree.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/vsprintf.o: kernel/lib/vsprintf.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/initcall.o: kernel/core/initcall.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/wait.o: kernel/core/wait.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/workqueue.o: kernel/core/workqueue.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/timer.o: kernel/core/timer.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/completion.o: kernel/core/completion.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/idr.o: kernel/core/idr.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD) noctua.iso iso/boot/noctua.bin

hdd.img:
	dd if=/dev/zero of=$@ bs=1M count=64
	parted $@ mklabel msdos mkpart primary fat32 1s 100% set 1 boot on
	LOOP=$$(losetup --show -f -P $@); \
	sleep 1; \
	mkfs.fat -F32 $$LOOPp1; \
	mount $$LOOPp1 /mnt; \
	echo "Chao mung ban den voi Noctua OS!" > /mnt/readme.txt; \
	mkdir -p /mnt/bin /mnt/home /mnt/home/user /mnt/system /mnt/tmp; \
	cp /mnt/readme.txt /mnt/home/user/; \
	echo "Noctua OS v1.0" > /mnt/system/version; \
	echo "default" > /mnt/system/config; \
	echo "hello world" > /mnt/bin/hello.txt; \
	umount /mnt; \
	losetup -d $$LOOP

.PHONY: all iso run run-serial run-iso clean window-test

MAKEFLAGS += -s --no-print-directory

all: iso

tools:
	@test -d tools || mkdir tools
	@test -d tools/limine || ( \
		cd tools && \
		git clone --depth=1 https://github.com/limine-bootloader/limine && \
		cd limine && \
		./bootstrap && \
		./configure --enable-bios --enable-uefi-x86-64 --enable-uefi-cd --enable-bios-cd CC=gcc CC_FOR_TARGET=gcc && \
		$(MAKE) \
	)

kernel:
	@$(MAKE) -C kernel

rootfs:
	@printf "  %-7s %s\n" "MKTAR" "rootfs.tar.gz"
	@test -d rootfs || (\
		mkdir rootfs && \
		echo "hello mate" > rootfs/test.txt && \
		mkdir rootfs/subdir && \
		echo "hello mate this is a subdir" > rootfs/subdir/lol \
	)
	@cp test.elf rootfs
	@tar -czvf rootfs.tar.gz --format=ustar -C rootfs .

iso: tools kernel rootfs
	@printf "  %-7s %s\n" "MKISO" "system.iso"
	@test -d iso || mkdir iso
	@test -d iso/EFI || mkdir iso/EFI
	@test -d iso/EFI/BOOT || mkdir iso/EFI/BOOT
	@cp tools/limine/bin/limine-bios.sys tools/limine/bin/limine-bios-cd.bin tools/limine/bin/limine-uefi-cd.bin iso
	@cp tools/limine/bin/BOOTX64.EFI iso/EFI/BOOT
	@cp kernel/kernel.elf iso
	@cp rootfs.tar.gz iso
	@cp config/limine.conf iso
	@xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label iso -o system.iso

run:
	@qemu-system-x86_64 -cdrom system.iso -enable-kvm -smp 1 -m 512

clean:
	@printf "  %-7s %s\n" "CLEAN" "rootfs.tar.gz system.iso"
	@rm -rf rootfs.tar.gz system.iso
	@$(MAKE) -C kernel clean

mrproper: clean
	@rm -rf tools rootfs iso

.PHONY: all tools kernel rootfs iso clean mrproper

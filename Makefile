CC := clang
EFI_LD := lld-link
KCC := x86_64-elf-gcc
KCXX := x86_64-elf-g++
KLD := x86_64-elf-ld
QEMU := qemu-system-x86_64

QEMU_SHARE := $(shell brew --cellar qemu)/$(shell brew list --versions qemu | awk '{print $$2}')/share/qemu

OVMF_CODE := $(QEMU_SHARE)/edk2-x86_64-code.fd
OVMF_VARS_TEMPLATE := $(QEMU_SHARE)/edk2-i386-vars.fd

BUILD := build
ESP := $(BUILD)/esp

EFI := $(BUILD)/BOOTX64.EFI
BOOT_OBJ := $(BUILD)/bootloader.obj

KERNEL_OBJ := $(BUILD)/kernel.o
KERNEL_ELF := $(BUILD)/kernel.elf

OVMF_VARS := $(BUILD)/OVMF_VARS.fd

BOOT_MAIN_C := boot/main.c
KERNEL_MAIN_C := kernel/kernel.cpp
KERNEL_LINKER_FILE := kernel/linker.ld

.PHONY: all run clean check esp

all: $(EFI) $(KERNEL_ELF) esp

$(BUILD):
	mkdir -p $(BUILD)

# --------------------------------------------------
# UEFI bootloader
# --------------------------------------------------

$(BOOT_OBJ): $(BOOT_MAIN_C) | $(BUILD)
	$(CC) --target=x86_64-pc-windows-msvc \
		-std=c11 \
		-O2 \
		-ffreestanding \
		-fno-stack-protector \
		-fno-builtin \
		-Wall -Wextra \
		-c $< -o $@

$(EFI): $(BOOT_OBJ)
	$(EFI_LD) /subsystem:efi_application \
		/entry:efi_main \
		/nodefaultlib \
		/machine:x64 \
		/out:$@ \
		$(BOOT_OBJ)

# --------------------------------------------------
# Kernel
# --------------------------------------------------

$(KERNEL_OBJ): $(KERNEL_MAIN_C) | $(BUILD)
	$(KCXX) \
		-std=c++20 \
		-O2 \
		-ffreestanding \
		-fno-stack-protector \
		-fno-builtin \
		-fno-exceptions \
		-fno-rtti \
		-fno-threadsafe-statics \
		-fno-use-cxa-atexit \
		-fno-pic \
		-fno-pie \
		-fno-asynchronous-unwind-tables \
		-fno-unwind-tables \
		-mno-red-zone \
		-mcmodel=small \
		-Wall -Wextra -Wpedantic \
		-c $< -o $@

$(KERNEL_ELF): $(KERNEL_OBJ) $(KERNEL_LINKER_FILE)
	$(KLD) \
		-nostdlib \
		-static \
		-z max-page-size=0x1000 \
		-T $(KERNEL_LINKER_FILE) \
		-o $@ \
		$(KERNEL_OBJ)

# --------------------------------------------------
# EFI filesystem
# --------------------------------------------------

esp: $(EFI) $(KERNEL_ELF)
	mkdir -p $(ESP)/EFI/BOOT
	cp $(EFI) $(ESP)/EFI/BOOT/BOOTX64.EFI
	cp $(KERNEL_ELF) $(ESP)/kernel.elf

# --------------------------------------------------
# UEFI variables
# --------------------------------------------------

$(OVMF_VARS): $(OVMF_VARS_TEMPLATE) | $(BUILD)
	cp $(OVMF_VARS_TEMPLATE) $(OVMF_VARS)

# --------------------------------------------------
# Validation
# --------------------------------------------------

check: all
	file $(EFI)
	file $(KERNEL_ELF)
	@echo "Bootloader and kernel built successfully."

# --------------------------------------------------
# Run in QEMU
# --------------------------------------------------

run: all $(OVMF_VARS)
	@test -f "$(OVMF_CODE)" || (echo "ERROR: UEFI code image not found"; exit 1)
	$(QEMU) \
		-machine q35 \
		-m 256M \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(OVMF_VARS) \
		-drive format=raw,file=fat:rw:$(ESP) \
		-serial stdio \
		-monitor none \
		-boot menu=on

# --------------------------------------------------
# Clean
# --------------------------------------------------

clean:
	rm -rf $(BUILD)

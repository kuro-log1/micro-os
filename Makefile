CC     = x86_64-elf-gcc
LD     = x86_64-elf-ld
AS     = x86_64-elf-as
MKRSC  = i686-elf-grub-mkrescue

CFLAGS  = -std=c11 -ffreestanding -O2 -Wall -Wextra \
          -fno-builtin -fno-stack-protector -fno-pic -m64 -I. \
          -fno-exceptions -fno-asynchronous-unwind-tables \
          -mno-sse -mno-sse2 -mno-mmx -mno-avx -mno-red-zone
ASFLAGS = --64
LDFLAGS = -T linker.ld -m elf_x86_64 --nostdlib

BUILD   = build
ISO_DIR = iso
KERNEL  = $(ISO_DIR)/boot/kernel.elf
ISO     = micro.iso

C_SRCS  = $(shell find kernel drivers lib mm proc shell fs -name '*.c' 2>/dev/null)
S_SRCS  = $(shell find boot kernel proc          -name '*.s' 2>/dev/null)
C_OBJS  = $(addprefix $(BUILD)/, $(C_SRCS:.c=.o))
S_OBJS  = $(addprefix $(BUILD)/, $(S_SRCS:.s=.o))
OBJS    = $(S_OBJS) $(C_OBJS)

.PHONY: all clean qemu

all: $(ISO)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL): $(OBJS) linker.ld
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

$(ISO_DIR)/boot/grub/grub.cfg: grub.cfg
	@mkdir -p $(dir $@)
	cp $< $@

$(ISO): $(KERNEL) $(ISO_DIR)/boot/grub/grub.cfg
	$(MKRSC) -o $@ $(ISO_DIR)

clean:
	rm -rf $(BUILD) $(KERNEL) $(ISO)

qemu: $(ISO)
	qemu-system-x86_64 \
	    -cdrom $(ISO) \
	    -m 512M \
	    -serial stdio \
	    -display cocoa,zoom-to-fit=on

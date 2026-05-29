CROSS_COMPILE ?= aarch64-none-elf-
CC      := $(CROSS_COMPILE)gcc
CXX     := $(CROSS_COMPILE)g++
AS      := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
DTC     ?= dtc

OUT     := output
BUILD   := build

# Include paths (matches BUILD "includes" attributes)
INCDIRS := -Iinclude -Isrc/arch/aarch64/include -Ithird_party/libfdt -I.

# --- copts per BUILD target ---

# start_head (src/arch/aarch64/*.c)
ARCH_CFLAGS   := -Wall -g -fno-stack-protector

# hyper-core (src/core/*.c, src/main.c)
CORE_CFLAGS   := -Wall -ffreestanding -fno-stack-protector -fno-builtin -O0 -g

# utils (src/utils/*.c)
UTIL_CFLAGS   := -Wall

# drivers (src/drivers/**/*.c)
DRIVER_CFLAGS := -Wall

# cxx-runtime (src/cxx_core/*.cc)
CXXFLAGS      := -Wall -Wextra -Wno-unused-parameter -ffreestanding -fno-stack-protector \
                 -fno-builtin -fno-exceptions -fno-rtti -fno-threadsafe-statics -O0 -g

# assembly (src/arch/aarch64/*.S)
ASFLAGS       := -D__ASSEMBLY__ -Wall -g

# third_party/libfdt
LIBFDT_CFLAGS := -Wall

# linker
LDFLAGS       := -nostdlib -nostartfiles \
                 -T $(BUILD)/linker.lds -Wl,--build-id=none

# --- Source files ---
ARCH_SRCS   := $(wildcard src/arch/aarch64/*.c)
CORE_SRCS   := $(wildcard src/core/*.c) src/main.c
UTIL_SRCS   := $(wildcard src/utils/*.c)
DRIVER_SRCS := $(wildcard src/drivers/gic/*.c) $(wildcard src/drivers/pl011/*.c)
CXX_SRCS    := $(wildcard src/cxx_core/*.cc)
AS_SRCS     := $(wildcard src/arch/aarch64/*.S)
LIBFDT_SRCS := $(wildcard third_party/libfdt/*.c)

# --- Object files ---
ARCH_OBJS   := $(patsubst %.c,$(BUILD)/%.o,$(ARCH_SRCS))
CORE_OBJS   := $(patsubst %.c,$(BUILD)/%.o,$(CORE_SRCS))
UTIL_OBJS   := $(patsubst %.c,$(BUILD)/%.o,$(UTIL_SRCS))
DRIVER_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(DRIVER_SRCS))
CXX_OBJS    := $(patsubst %.cc,$(BUILD)/%.o,$(CXX_SRCS))
AS_OBJS     := $(patsubst %.S,$(BUILD)/%.o,$(AS_SRCS))
LIBFDT_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(LIBFDT_SRCS))

OBJS := $(AS_OBJS) $(ARCH_OBJS) $(CORE_OBJS) $(UTIL_OBJS) \
        $(DRIVER_OBJS) $(CXX_OBJS) $(LIBFDT_OBJS)

# --- Targets ---
.PHONY: all clean

all: $(OUT)/hyper-elf $(OUT)/core.bin $(OUT)/hyper.dtb

$(OUT)/hyper-elf: $(OBJS) $(BUILD)/linker.lds
	@mkdir -p $(OUT)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) -lc -lgcc

$(OUT)/core.bin: $(OUT)/hyper-elf
	$(OBJCOPY) -O binary $< $@

$(OUT)/hyper.dtb: hyper.dts
	@mkdir -p $(OUT)
	dtc -O dtb $< -o $@

# Preprocess linker script
$(BUILD)/linker.lds: src/arch/aarch64/linker.lds include/config.h
	@mkdir -p $(dir $@)
	$(CC) -E -x c -Iinclude $< | grep -v '^#' > $@

# --- Compile rules ---

# Assembly
$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $(INCDIRS) -c -o $@ $<

# Arch sources
$(BUILD)/src/arch/aarch64/%.o: src/arch/aarch64/%.c
	@mkdir -p $(dir $@)
	$(CC) $(ARCH_CFLAGS) $(INCDIRS) -c -o $@ $<

# Core sources
$(BUILD)/src/core/%.o: src/core/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CORE_CFLAGS) $(INCDIRS) -c -o $@ $<

$(BUILD)/src/main.o: src/main.c
	@mkdir -p $(dir $@)
	$(CC) $(CORE_CFLAGS) $(INCDIRS) -c -o $@ $<

# Utils
$(BUILD)/src/utils/%.o: src/utils/%.c
	@mkdir -p $(dir $@)
	$(CC) $(UTIL_CFLAGS) $(INCDIRS) -c -o $@ $<

# Drivers
$(BUILD)/src/drivers/%.o: src/drivers/%.c
	@mkdir -p $(dir $@)
	$(CC) $(DRIVER_CFLAGS) $(INCDIRS) -c -o $@ $<

# C++
$(BUILD)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCDIRS) -c -o $@ $<

# libfdt
$(BUILD)/third_party/%.o: third_party/%.c
	@mkdir -p $(dir $@)
	$(CC) $(LIBFDT_CFLAGS) $(INCDIRS) -c -o $@ $<

clean:
	rm -rf $(BUILD) $(OUT)

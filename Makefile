TARGET ?= aarch64
SUPPORTED_TARGETS := aarch64 riscv64

ifneq ($(filter $(TARGET),$(SUPPORTED_TARGETS)),$(TARGET))
$(error unsupported TARGET '$(TARGET)'; use one of: $(SUPPORTED_TARGETS))
endif

ifeq ($(TARGET),aarch64)
DEFAULT_CROSS_COMPILE := aarch64-none-elf-
ARCH_DIR := src/arch/aarch64
LINKER_SCRIPT := $(ARCH_DIR)/linker.lds
ARCH_COPTS :=
ARCH_LINKOPTS := -Wl,--no-warn-rwx-segments
ARCH_DRIVER_SRCS := $(wildcard src/drivers/gic/*.cc) $(wildcard src/drivers/pl011/*.cc)
ARCH_CORE_EXCLUDES :=
ARCH_CORE_EXTRAS :=
ARCH_CC_EXCLUDES :=
else ifeq ($(TARGET),riscv64)
DEFAULT_CROSS_COMPILE := riscv-none-elf-
ARCH_DIR := src/arch/riscv64
LINKER_SCRIPT := $(ARCH_DIR)/linker.ld
ARCH_COPTS := -mcmodel=medany -march=rv64imac_zicsr -mabi=lp64
ARCH_LINKOPTS := -march=rv64imac_zicsr -mabi=lp64 -Wl,-m,elf64lriscv
ARCH_DRIVER_SRCS := $(wildcard src/drivers/pl011/*.cc)
ARCH_CORE_EXCLUDES := \
    src/core/emul_gic.cc \
    src/core/emul_gicv3.cc \
    src/core/emul_uart.cc \
    src/core/emul_psci.cc \
    src/core/sched.cc
ARCH_CORE_EXTRAS := src/arch/riscv64/sched_riscv.cc
ARCH_CC_EXCLUDES := src/arch/riscv64/sched_riscv.cc
endif

CROSS_COMPILE ?= $(DEFAULT_CROSS_COMPILE)
CC      := $(CROSS_COMPILE)gcc
CXX     := $(CROSS_COMPILE)g++
AS      := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
DTC     ?= dtc

OUT ?= output/$(TARGET)
BUILD_DIR ?= build/$(TARGET)

INCDIRS := \
    -Iinclude \
    -I$(ARCH_DIR)/include \
    -Ithird_party/libfdt \
    -I.

COMMON_CFLAGS := \
    -Wall \
    -D_POSIX_C_SOURCE=200809L \
    -ffreestanding \
    -fno-stack-protector \
    -fno-builtin \
    -O0 \
    -g \
    $(ARCH_COPTS)

COMMON_CXXFLAGS := \
    -std=gnu++17 \
    -Wall \
    -Wextra \
    -Wno-unused-parameter \
    -D_POSIX_C_SOURCE=200809L \
    -ffreestanding \
    -fno-stack-protector \
    -fno-builtin \
    -fno-exceptions \
    -fno-rtti \
    -fno-threadsafe-statics \
    -O0 \
    -g \
    $(ARCH_COPTS)

ASFLAGS := -D__ASSEMBLY__ -Wall -g $(ARCH_COPTS)
LDFLAGS := \
    -nostdlib \
    -nostartfiles \
    -nodefaultlibs \
    -T $(BUILD_DIR)/linker.lds \
    -Wl,--build-id=none \
    $(ARCH_LINKOPTS)
LDLIBS := -lc -lgcc

ARCH_CC_SRCS := $(filter-out $(ARCH_CC_EXCLUDES),$(wildcard $(ARCH_DIR)/*.cc))
AS_SRCS := $(wildcard $(ARCH_DIR)/*.S)
CORE_SRCS := $(filter-out $(ARCH_CORE_EXCLUDES),$(wildcard src/core/*.cc)) $(ARCH_CORE_EXTRAS) src/main.cc
CXX_RUNTIME_SRCS := $(wildcard src/cxx_core/*.cc)
UTIL_SRCS := $(wildcard src/utils/*.c) $(wildcard src/utils/*.cc)
LIBFDT_SRCS := $(wildcard third_party/libfdt/*.c)

SRCS := \
    $(AS_SRCS) \
    $(ARCH_CC_SRCS) \
    $(CORE_SRCS) \
    $(CXX_RUNTIME_SRCS) \
    $(UTIL_SRCS) \
    $(ARCH_DRIVER_SRCS) \
    $(LIBFDT_SRCS)

OBJS := $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(SRCS)))
DEPS := $(OBJS:.o=.d)

.PHONY: all hyper clean check-tools print-config help aarch64 riscv64

all: hyper

hyper: $(OUT)/hyper-elf $(OUT)/core.bin $(OUT)/hyper.dtb

$(OUT)/hyper-elf: check-tools $(OBJS) $(BUILD_DIR)/linker.lds
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OUT)/core.bin: $(OUT)/hyper-elf
	$(OBJCOPY) -O binary $< $@

$(OUT)/hyper.dtb: hyper.dts
	@mkdir -p $(dir $@)
	$(DTC) -O dtb $< -o $@

$(BUILD_DIR)/linker.lds: $(LINKER_SCRIPT) include/config.h
	@mkdir -p $(dir $@)
	$(CC) $(ARCH_COPTS) -E -x c -Iinclude $< | grep -v '^#' > $@

$(BUILD_DIR)/%.S.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $(INCDIRS) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(COMMON_CFLAGS) $(INCDIRS) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/%.cc.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $(COMMON_CXXFLAGS) $(INCDIRS) -MMD -MP -c -o $@ $<

check-tools:
	@command -v $(CC) >/dev/null || { echo "missing compiler: $(CC)"; exit 1; }
	@command -v $(CXX) >/dev/null || { echo "missing compiler: $(CXX)"; exit 1; }
	@command -v $(OBJCOPY) >/dev/null || { echo "missing objcopy: $(OBJCOPY)"; exit 1; }
	@command -v $(DTC) >/dev/null || { echo "missing dtc: $(DTC)"; exit 1; }

aarch64:
	$(MAKE) TARGET=aarch64

riscv64:
	$(MAKE) TARGET=riscv64

print-config:
	@printf 'TARGET=%s\n' '$(TARGET)'
	@printf 'CROSS_COMPILE=%s\n' '$(CROSS_COMPILE)'
	@printf 'CC=%s\n' '$(CC)'
	@printf 'CXX=%s\n' '$(CXX)'
	@printf 'OUT=%s\n' '$(OUT)'
	@printf 'BUILD_DIR=%s\n' '$(BUILD_DIR)'
	@printf 'LINKER_SCRIPT=%s\n' '$(LINKER_SCRIPT)'

help:
	@printf 'Usage:\n'
	@printf '  make TARGET=aarch64 [CROSS_COMPILE=aarch64-none-elf-]\n'
	@printf '  make TARGET=riscv64 [CROSS_COMPILE=riscv-none-elf-]\n'
	@printf '  make aarch64\n'
	@printf '  make riscv64\n'
	@printf '  make clean\n'

clean:
	rm -rf build output

-include $(DEPS)

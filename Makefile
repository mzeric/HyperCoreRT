ifneq ("$(origin O)", "command line")
O := $(CURDIR)/build
endif

CANONICAL_O := $(shell mkdir -p $(O) >/dev/null 2>&1)$(realpath $(O))
BASE_DIR := $(CANONICAL_O)

$(if $(BASE_DIR),, $(error output directory "$(O)" does not exist))
BUILD_DIR := $(BASE_DIR)/kconf_build


.PHONY: outputmakefile
outputmakefile:

CONFIG_CONFIG_IN=Config.in
KCONFIG=scripts/kconfig
HOSTCC_NOCCACHE=gcc

$(BUILD_DIR)/hyper-config/%onf:
	mkdir -p $(@D)/lxdialog
	PKG_CONFIG_PATH="$(HOST_PKG_CONFIG_PATH)" $(MAKE) CC="$(HOSTCC_NOCCACHE)" HOSTCC="$(HOSTCC_NOCCACHE)" \
	    obj=$(@D) -C $(KCONFIG) -f Makefile.br $(@F)

COMMON_CONFIG_ENV = \
	BR2_DEFCONFIG='$(call qstrip,$(value BR2_DEFCONFIG))' \
	KCONFIG_AUTOCONFIG=$(BUILD_DIR)/hyper-config/auto.conf \
	KCONFIG_AUTOHEADER=$(CURDIR)/include/autoconf.h \
	KCONFIG_TRISTATE=$(BUILD_DIR)/hyper-config/tristate.config \
	BR2_CONFIG=$(CURDIR)/.config \
	HOST_GCC_VERSION="$(HOSTCC_VERSION)" \
	BASE_DIR=$(BASE_DIR) \
	SKIP_LEGACY=

menuconfig: $(BUILD_DIR)/hyper-config/mconf outputmakefile
	@$(COMMON_CONFIG_ENV) $< $(CONFIG_CONFIG_IN)


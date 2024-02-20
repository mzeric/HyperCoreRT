ifneq ("$(origin O)", "command line")
O := $(CURDIR)/build
endif

CANONICAL_O := $(shell mkdir -p $(O) >/dev/null 2>&1)$(realpath $(O))
BASE_DIR := $(CANONICAL_O)

$(if $(BASE_DIR),, $(error output directory "$(O)" does not exist))
BUILD_DIR := $(BASE_DIR)/kconfig-frontends

KCONFIG_DIR=${BUILD_DIR}/kconfig/frontends

.PHONY: outputmakefile
outputmakefile:

CONFIG_CONFIG_IN=Config.in
HOSTCC_NOCCACHE=gcc

$(KCONFIG_DIR)/mconf/kconfig-mconf:
	./scripts/build_kconfig.sh

COMMON_CONFIG_ENV = \
	BR2_DEFCONFIG='$(call qstrip,$(value BR2_DEFCONFIG))' \
	KCONFIG_AUTOCONFIG=$(BUILD_DIR)/auto.conf \
	KCONFIG_AUTOHEADER=$(CURDIR)/include/autoconf.h \
	KCONFIG_TRISTATE=$(BUILD_DIR)/tristate.config \
	BR2_CONFIG=$(CURDIR)/.config \
	HOST_GCC_VERSION="$(HOSTCC_VERSION)" \
	BASE_DIR=$(BASE_DIR) \
	SKIP_LEGACY=

menuconfig: $(KCONFIG_DIR)/mconf/kconfig-mconf outputmakefile
	@$(COMMON_CONFIG_ENV) $< $(CONFIG_CONFIG_IN)
	@mkdir include/config -p
	@$(COMMON_CONFIG_ENV) $(KCONFIG_DIR)/conf/kconfig-conf --silentoldconfig $(CONFIG_CONFIG_IN)

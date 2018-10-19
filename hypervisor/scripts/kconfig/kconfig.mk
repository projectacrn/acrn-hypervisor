# usage: override_config <symbol> <default>
#
# Given a configuration symbol (without the CONFIG_ prefix), this macro
# overrides its value as follows.
#    1. If a value is specified from command line, that value is used.
#    2. If neither config.mk nor the command line specifies a value, the given
#       default is used.
define override_config =
ifdef $(1)
CONFIG_$(1) := $($(1))
else ifndef CONFIG_$(1)
CONFIG_$(1) := $(2)
endif
endef

HV_CONFIG := .config
HV_DEFCONFIG := defconfig
HV_CONFIG_H := include/config.h
HV_CONFIG_MK := include/config.mk

KCONFIG_DIR := $(BASEDIR)/../scripts/kconfig

# Backward-compatibility for RELEASE=(0|1)
ifdef RELEASE
ifeq ($(RELEASE),1)
override RELEASE := y
else
override RELEASE := n
endif
endif

-include $(HV_OBJDIR)/$(HV_CONFIG_MK)
$(eval $(call override_config,PLATFORM,sbl))
$(eval $(call override_config,RELEASE,n))

$(eval $(call check_dep_exec,python3,KCONFIG_DEPS))
$(eval $(call check_dep_py3lib,kconfiglib,KCONFIG_DEPS))

# This target invoke silentoldconfig to generate or update a .config. Useful as
# a prerequisite of other targets depending on .config.
$(HV_OBJDIR)/$(HV_CONFIG): oldconfig

# Note: This target must not depend on a phony target (e.g. oldconfig) because
# it'll trigger endless re-execution of make.
$(HV_OBJDIR)/$(HV_CONFIG_MK): $(HV_OBJDIR)/$(HV_CONFIG)
	@mkdir -p $(dir $@)
	@sed 's/="\(.*\)"$$/=\1/g' $(HV_OBJDIR)/$(HV_CONFIG) > $@

$(HV_OBJDIR)/$(HV_CONFIG_H): $(HV_OBJDIR)/$(HV_CONFIG)
	@mkdir -p $(dir $@)
	@python3 $(KCONFIG_DIR)/generate_header.py Kconfig $< $@

# This target forcefully generate a .config based on a given default
# one. Overwrite the current .config if it exists.
.PHONY: defconfig
defconfig: $(KCONFIG_DEPS)
	@mkdir -p $(HV_OBJDIR)
	@python3 $(KCONFIG_DIR)/defconfig.py Kconfig \
		arch/x86/configs/$(CONFIG_PLATFORM).config \
		$(HV_OBJDIR)/$(HV_CONFIG)

# Use silentoldconfig to forcefully update the current .config, or generate a
# new one if no previous .config exists. This target can be used as a
# prerequisite of all the others to make sure that the .config is consistent
# even it has been modified manually before.
#
# Note: Should not pass CONFIG_xxx to silentoldconfig here because config.mk can
# be out-dated.
.PHONY: oldconfig
oldconfig: $(KCONFIG_DEPS)
	@mkdir -p $(HV_OBJDIR)
	@python3 $(KCONFIG_DIR)/silentoldconfig.py Kconfig \
		$(HV_OBJDIR)/$(HV_CONFIG) \
		PLATFORM_$(shell echo $(PLATFORM) | tr a-z A-Z)=y \
		RELEASE=$(RELEASE)

# Minimize the current .config. This target can be used to generate a defconfig
# for future use.
.PHONY: savedefconfig
savedefconfig: $(HV_OBJDIR)/$(HV_CONFIG)
	@python3 $(KCONFIG_DIR)/savedefconfig.py Kconfig \
		$(HV_OBJDIR)/$(HV_CONFIG) \
		$(HV_OBJDIR)/$(HV_DEFCONFIG)

$(eval $(call check_dep_exec,menuconfig,MENUCONFIG_DEPS))
export KCONFIG_CONFIG := $(HV_OBJDIR)/$(HV_CONFIG)
menuconfig: $(MENUCONFIG_DEPS) $(HV_OBJDIR)/$(HV_CONFIG)
	@python3 $(shell which menuconfig) Kconfig

CFLAGS += -include $(HV_OBJDIR)/$(HV_CONFIG_H)

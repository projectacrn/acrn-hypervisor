HV_CONFIG := .config
HV_DEFCONFIG := defconfig
HV_CONFIG_H := include/config.h
HV_CONFIG_MK := include/config.mk

KCONFIG_DIR := $(BASEDIR)/../scripts/kconfig

$(eval $(call check_dep_exec,python))
$(eval $(call check_dep_exec,pip))
$(eval $(call check_dep_pylib,kconfiglib))

# This target invoke silentoldconfig to generate or update a .config. Useful as
# a prerequisite of other targets depending on .config.
$(HV_OBJDIR)/$(HV_CONFIG): oldconfig

# Note: This target must not depend on a phony target (e.g. oldconfig) because
# it'll trigger endless re-execution of make.
$(HV_OBJDIR)/$(HV_CONFIG_MK): $(HV_OBJDIR)/$(HV_CONFIG)
	@mkdir -p $(dir $@)
	@cp $< $@

$(HV_OBJDIR)/$(HV_CONFIG_H): $(HV_OBJDIR)/$(HV_CONFIG)
	@mkdir -p $(dir $@)
	@python $(KCONFIG_DIR)/generate_header.py Kconfig $< $@

# This target forcefully generate a .config based on a given default
# one. Overwrite the current .config if it exists.
.PHONY: defconfig
defconfig:
	@mkdir -p $(HV_OBJDIR)
	@python $(KCONFIG_DIR)/defconfig.py Kconfig arch/x86/configs/$(PLATFORM).config $(HV_OBJDIR)/$(HV_CONFIG)

# Use silentoldconfig to forcefully update the current .config, or generate a
# new one if no previous .config exists. This target can be used as a
# prerequisite of all the others to make sure that the .config is consistent
# even it has been modified manually before.
.PHONY: oldconfig
oldconfig:
	@mkdir -p $(HV_OBJDIR)
	@python $(KCONFIG_DIR)/silentoldconfig.py Kconfig $(HV_OBJDIR)/$(HV_CONFIG) PLATFORM_$(shell echo $(PLATFORM) | tr a-z A-Z)=y

# Minimize the current .config. This target can be used to generate a defconfig
# for future use.
.PHONY: minimalconfig
minimalconfig: $(HV_OBJDIR)/$(HV_CONFIG)
	@python $(KCONFIG_DIR)/minimalconfig.py Kconfig $(HV_OBJDIR)/$(HV_CONFIG) $(HV_OBJDIR)/$(HV_DEFCONFIG)

-include $(HV_OBJDIR)/$(HV_CONFIG_MK)

CFLAGS += -include $(HV_OBJDIR)/$(HV_CONFIG_H)

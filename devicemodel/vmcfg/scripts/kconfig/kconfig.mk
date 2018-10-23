$(eval $(call check_dep_exec,menuconfig,MENUCONFIG_DEPS))

export KCONFIG_CONFIG := $(DM_OBJDIR)/vmcfg/.config

.PHONY: oldconfig
oldconfig:
	mkdir -p $(dir $(KCONFIG_CONFIG))
	@python3 $(BASEDIR)/../scripts/kconfig/silentoldconfig.py Kconfig $(KCONFIG_CONFIG)

%_defconfig:
	mkdir -p $(dir $(KCONFIG_CONFIG))
	@python3 $(BASEDIR)/../scripts/kconfig/defconfig.py Kconfig $(BASEDIR)/vmcfg/config/$@ $(KCONFIG_CONFIG)

$(KCONFIG_CONFIG): oldconfig

$(DM_OBJDIR)/include/vmcfg_config.h: $(KCONFIG_CONFIG)
	echo @mkdir -p $(dir $@)
	@mkdir -p $(dir $@)
	@python3 $(BASEDIR)/../scripts/kconfig/generate_header.py Kconfig $< $@

menuconfig: $(MENUCONFIG_DEPS) $(HV_OBJDIR)/$(HV_CONFIG) $(KCONFIG_CONFIG)
	@python3 $(shell which menuconfig) Kconfig $(KCONFIG_CONFIG)

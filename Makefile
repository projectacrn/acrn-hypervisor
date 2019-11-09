
# global helper variables
T := $(CURDIR)

BOARD ?= kbl-nuc-i7

ifneq (,$(filter $(BOARD),apl-mrb))
	FIRMWARE ?= sbl
else
	FIRMWARE ?= uefi
endif

RELEASE ?= 0
SCENARIO ?= sdc

O ?= build
ROOT_OUT := $(shell mkdir -p $(O);cd $(O);pwd)
HV_OUT := $(ROOT_OUT)/hypervisor
EFI_OUT := misc/efi-stub
DM_OUT := $(ROOT_OUT)/devicemodel
TOOLS_OUT := $(ROOT_OUT)/misc/tools
DOC_OUT := $(ROOT_OUT)/doc
BUILD_VERSION ?=
BUILD_TAG ?=

export TOOLS_OUT BOARD SCENARIO FIRMWARE RELEASE

.PHONY: all hypervisor devicemodel tools doc
all: hypervisor devicemodel tools

ifeq ($(BOARD), apl-nuc)
override BOARD := nuc6cayh
else ifeq ($(BOARD), kbl-nuc-i7)
override BOARD := nuc7i7dnb
endif

include $(T)/misc/acrn-config/library/cfg_update.mk

#help functions to build acrn and install acrn/acrn symbols
define build_acrn
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-$(1)/$(2) BOARD=$(2) FIRMWARE=$(1) SCENARIO=$(4) RELEASE=$(RELEASE) clean
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-$(1)/$(2) BOARD=$(2) FIRMWARE=$(1) SCENARIO=$(4) RELEASE=$(RELEASE) defconfig
	@echo "$(3)=y" >> $(HV_OUT)-$(1)/$(2)/.config
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-$(1)/$(2) BOARD=$(2) FIRMWARE=$(1) SCENARIO=$(4) RELEASE=$(RELEASE) oldconfig
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-$(1)/$(2) BOARD=$(2) FIRMWARE=$(1) SCENARIO=$(4) RELEASE=$(RELEASE)
	echo "building hypervisor as EFI executable..."
	@if [ "$(1)" = "uefi" ]; then \
	$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT)-$(1)/$(2) SCENARIO=$(4) EFI_OBJDIR=$(HV_OUT)-$(1)/$(2)/$(EFI_OUT); \
	fi
endef

define install_acrn
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-$(1)/$(2) BOARD=$(2) FIRMWARE=$(1) SCENARIO=$(3) RELEASE=$(RELEASE) install
	@if [ "$(1)" = "uefi" ]; then \
	$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT)-$(1)/$(2) BOARD=$(2) FIRMWARE=$(1) SCENARIO=$(3) RELEASE=$(RELEASE) EFI_OBJDIR=$(HV_OUT)-$(1)/$(2)/$(EFI_OUT) install; \
	fi
endef

define install_acrn_debug
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-$(1)/$(2) BOARD=$(2) FIRMWARE=$(1) SCENARIO=$(3) RELEASE=$(RELEASE) install-debug
	@if [ "$(1)" = "uefi" ]; then \
	$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT)-$(1)/$(2) BOARD=$(2) FIRMWARE=$(1) SCENARIO=$(3) RELEASE=$(RELEASE) EFI_OBJDIR=$(HV_OUT)-$(1)/$(2)/$(EFI_OUT) install-debug; \
	fi
endef

hypervisor:
	@if [ "$(SCENARIO)" != "sdc" ] && [ "$(SCENARIO)" != "sdc2" ] && [ "$(SCENARIO)" != "industry" ] \
			&& [ "$(SCENARIO)" != "logical_partition" ] && [ "$(SCENARIO)" != "hybrid" ]; then \
		echo "SCENARIO <$(SCENARIO)> is not supported. "; exit 1; \
	fi
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) clean
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) defconfig
	@if [ "$(CONFIG_XML_ENABLED)" = "true" ]; then \
		echo "CONFIG_$(shell echo $(SCENARIO_IN_XML) | tr a-z A-Z)=y" >> $(HV_OUT)/.config;	\
		echo "CONFIG_ENFORCE_VALIDATED_ACPI_INFO=y" >> $(HV_OUT)/.config;	\
	elif [ "$(SCENARIO)" ]; then \
		echo "CONFIG_$(shell echo $(SCENARIO) | tr a-z A-Z)=y" >> $(HV_OUT)/.config;	\
	fi
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)
#ifeq ($(FIRMWARE),uefi)
	@if [ "$(SCENARIO)" != "logical_partition" ] && [ "$(SCENARIO)" != "hybrid" ]; then \
		echo "building hypervisor as EFI executable..."; \
		$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT) EFI_OBJDIR=$(HV_OUT)/$(EFI_OUT); \
	fi
#endif


devicemodel: tools
	$(MAKE) -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT) RELEASE=$(RELEASE) clean
	$(MAKE) -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT) DM_BUILD_VERSION=$(BUILD_VERSION) DM_BUILD_TAG=$(BUILD_TAG) DM_ASL_COMPILER=$(ASL_COMPILER) RELEASE=$(RELEASE)

tools:
	mkdir -p $(TOOLS_OUT)
	$(MAKE) -C $(T)/misc OUT_DIR=$(TOOLS_OUT) RELEASE=$(RELEASE)

doc:
	$(MAKE) -C $(T)/doc html BUILDDIR=$(DOC_OUT)

.PHONY: clean
clean:
	$(MAKE) -C $(T)/misc OUT_DIR=$(TOOLS_OUT) clean
	$(MAKE) -C $(T)/doc BUILDDIR=$(DOC_OUT) clean
	rm -rf $(ROOT_OUT)

.PHONY: install
install: hypervisor-install devicemodel-install tools-install

hypervisor-install:
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) SCENARIO=$(SCENARIO) RELEASE=$(RELEASE) install
ifeq ($(FIRMWARE),uefi)
	$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) SCENARIO=$(SCENARIO) EFI_OBJDIR=$(HV_OUT)/$(EFI_OUT) all install
endif

hypervisor-install-debug:
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) SCENARIO=$(SCENARIO) RELEASE=$(RELEASE) install-debug
ifeq ($(FIRMWARE),uefi)
	$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) SCENARIO=$(SCENARIO) EFI_OBJDIR=$(HV_OUT)/$(EFI_OUT) all install-debug
endif

apl-mrb-sbl-sdc:
	$(call build_acrn,sbl,apl-mrb,CONFIG_SDC,sdc)
apl-up2-sbl-sdc:
	$(call build_acrn,sbl,apl-up2,CONFIG_SDC,sdc)
kbl-nuc-i7-uefi-industry:
	$(call build_acrn,uefi,nuc7i7dnb,CONFIG_INDUSTRY,industry)
apl-up2-uefi-hybrid:
	$(call build_acrn,uefi,apl-up2,CONFIG_HYBRID,hybrid)

sbl-hypervisor: apl-mrb-sbl-sdc \
                apl-up2-sbl-sdc \
                kbl-nuc-i7-uefi-industry \
                apl-up2-uefi-hybrid

apl-mrb-sbl-sdc-install:
	$(call install_acrn,sbl,apl-mrb,sdc)
apl-up2-sbl-sdc-install:
	$(call install_acrn,sbl,apl-up2,sdc)
kbl-nuc-i7-uefi-industry-install:
	$(call install_acrn,uefi,nuc7i7dnb,industry)
apl-up2-uefi-hybrid-install:
	$(call install_acrn,uefi,apl-up2,hybrid)

sbl-hypervisor-install: apl-mrb-sbl-sdc-install \
                        apl-up2-sbl-sdc-install \
                        kbl-nuc-i7-uefi-industry-install \
                        apl-up2-uefi-hybrid-install

apl-mrb-sbl-sdc-install-debug:
	$(call install_acrn_debug,sbl,apl-mrb,sdc)
apl-up2-sbl-sdc-install-debug:
	$(call install_acrn_debug,sbl,apl-up2,sdc)
kbl-nuc-i7-uefi-industry-install-debug:
	$(call install_acrn_debug,uefi,nuc7i7dnb,industry)
apl-up2-uefi-hybrid-install-debug:
	$(call install_acrn_debug,uefi,apl-up2,hybrid)

sbl-hypervisor-install-debug: apl-mrb-sbl-sdc-install-debug \
			      apl-up2-sbl-sdc-install-debug \
			      kbl-nuc-i7-uefi-industry-install-debug \
			      apl-up2-uefi-hybrid-install-debug

devicemodel-install:
	$(MAKE) -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT) install

tools-install:
	$(MAKE) -C $(T)/misc OUT_DIR=$(TOOLS_OUT) RELEASE=$(RELEASE) install

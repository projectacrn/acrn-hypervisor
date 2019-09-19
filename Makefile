
# global helper variables
T := $(CURDIR)

BOARD ?= apl-nuc

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
EFI_OUT := $(ROOT_OUT)/misc/efi-stub
DM_OUT := $(ROOT_OUT)/devicemodel
TOOLS_OUT := $(ROOT_OUT)/misc/tools
DOC_OUT := $(ROOT_OUT)/doc
BUILD_VERSION ?=
BUILD_TAG ?=
BOARD_FILE ?=
SCENARIO_FILE ?=
CONFIG_XML_ENABLED ?= false

export TOOLS_OUT

.PHONY: all hypervisor devicemodel tools doc
all: hypervisor devicemodel tools

ifneq ($(BOARD_FILE)$(SCENARIO_FILE),)
override BOARD := $(shell echo `sed -n '/<acrn-config/p' $(BOARD_FILE) | sed -r 's/.*board="(.*)".*/\1/g'`)
override SCENARIO := $(shell echo `sed -n '/<acrn-config/p' $(SCENARIO_FILE) | sed -r 's/.*scenario="(.*)".*/\1/g'`)

ifneq ($(BOARD)$(SCENARIO),)
CONFIG_XML_ENABLED := true
endif
endif

ifeq ($(BOARD), apl-nuc)
override BOARD := nuc6cayh
else ifeq ($(BOARD), kbl-nuc-i7)
override BOARD := nuc7i7dnb
endif

hypervisor:
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) RELEASE=$(RELEASE)	\
				BOARD_FILE=$(BOARD_FILE) SCENARIO_FILE=$(SCENARIO_FILE) clean
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) RELEASE=$(RELEASE)	\
				BOARD_FILE=$(BOARD_FILE) SCENARIO_FILE=$(SCENARIO_FILE) defconfig
	@if [ "$(SCENARIO)" ]; then \
		echo "CONFIG_$(shell echo $(SCENARIO) | tr a-z A-Z)=y" >> $(HV_OUT)/.config;	\
	fi
	@if [ "$(CONFIG_XML_ENABLED)" = "true" ]; then \
		echo "CONFIG_ENFORCE_VALIDATED_ACPI_INFO=y" >> $(HV_OUT)/.config;	\
	fi
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) RELEASE=$(RELEASE)	\
				BOARD_FILE=$(BOARD_FILE) SCENARIO_FILE=$(SCENARIO_FILE) oldconfig
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) RELEASE=$(RELEASE)	\
				BOARD_FILE=$(BOARD_FILE) SCENARIO_FILE=$(SCENARIO_FILE)
ifeq ($(FIRMWARE),uefi)
	echo "building hypervisor as EFI executable..."
	$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT) SCENARIO=$(SCENARIO) EFI_OBJDIR=$(EFI_OUT)
endif

sbl-hypervisor:
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-mrb BOARD=apl-mrb FIRMWARE=sbl RELEASE=$(RELEASE) clean
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-mrb BOARD=apl-mrb FIRMWARE=sbl RELEASE=$(RELEASE) defconfig
	@echo "CONFIG_SDC=y" >> $(HV_OUT)-sbl/.config
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-mrb BOARD=apl-mrb FIRMWARE=sbl RELEASE=$(RELEASE) oldconfig
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-mrb BOARD=apl-mrb FIRMWARE=sbl RELEASE=$(RELEASE)

	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-up2 BOARD=apl-up2 FIRMWARE=sbl RELEASE=$(RELEASE) clean
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-up2 BOARD=apl-up2 FIRMWARE=sbl RELEASE=$(RELEASE) defconfig
	@echo "CONFIG_SDC=y" >> $(HV_OUT)-sbl/.config
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-up2 BOARD=apl-up2 FIRMWARE=sbl RELEASE=$(RELEASE) oldconfig
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-up2 BOARD=apl-up2 FIRMWARE=sbl RELEASE=$(RELEASE)

	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-isd BOARD=kbl-nuc-i7 FIRMWARE=uefi RELEASE=$(RELEASE) clean
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-isd BOARD=kbl-nuc-i7 FIRMWARE=uefi RELEASE=$(RELEASE) defconfig
	@echo "CONFIG_INDUSTRY=y" >> $(HV_OUT)-isd/.config
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-isd BOARD=kbl-nuc-i7 FIRMWARE=uefi RELEASE=$(RELEASE) oldconfig
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-isd BOARD=kbl-nuc-i7 FIRMWARE=uefi RELEASE=$(RELEASE)

ifeq ($(FIRMWARE),uefi)
	echo "building hypervisor as EFI executable..."
	$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT)-isd SCENARIO=industry EFI_OBJDIR=$(EFI_OUT)
endif

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
	$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) SCENARIO=$(SCENARIO) EFI_OBJDIR=$(EFI_OUT) all install
endif

hypervisor-install-debug:
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) SCENARIO=$(SCENARIO) RELEASE=$(RELEASE) install-debug
ifeq ($(FIRMWARE),uefi)
	$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) SCENARIO=$(SCENARIO) EFI_OBJDIR=$(EFI_OUT) all install-debug
endif

sbl-hypervisor-install:
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-mrb BOARD=apl-mrb FIRMWARE=sbl SCENARIO=sdc RELEASE=$(RELEASE) install
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-up2 BOARD=apl-up2 FIRMWARE=sbl SCENARIO=sdc RELEASE=$(RELEASE) install
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-isd BOARD=kbl-nuc-i7 FIRMWARE=uefi SCENARIO=industry RELEASE=$(RELEASE) install
	$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT)-isd BOARD=kbl-nuc-i7 FIRMWARE=uefi SCENARIO=industry EFI_OBJDIR=$(EFI_OUT) all install

sbl-hypervisor-install-debug:
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-mrb BOARD=apl-mrb FIRMWARE=sbl SCENARIO=sdc RELEASE=$(RELEASE) install-debug
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-up2 BOARD=apl-up2 FIRMWARE=sbl SCENARIO=sdc RELEASE=$(RELEASE) install-debug
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-isd BOARD=kbl-nuc-i7 FIRMWARE=uefi SCENARIO=industry RELEASE=$(RELEASE) install-debug
	$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT)-isd BOARD=kbl-nuc-i7 FIRMWARE=uefi SCENARIO=industry EFI_OBJDIR=$(EFI_OUT) all install-debug

devicemodel-install:
	$(MAKE) -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT) install

tools-install:
	$(MAKE) -C $(T)/misc OUT_DIR=$(TOOLS_OUT) RELEASE=$(RELEASE) install


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

export TOOLS_OUT

.PHONY: all hypervisor devicemodel tools doc
all: hypervisor devicemodel tools

ifneq ($(BOARD_FILE)$(SCENARIO_FILE),)
BOARD := `sed -n '/board/p' $(BOARD_FILE) |head -1|awk -F'"' '{print $$2}'`
SCENARIO := `sed -n '/scenario/p' $(SCENARIO_FILE) |head -1|awk -F'"' '{print $$4}'`

cfg_src:
	@if [ ! -f $(BOARD_FILE) ] ; then \
		echo "$(BOARD_FILE) is not exist!"; exit 1; \
	fi
	@if [ ! -f "$(SCENARIO_FILE)" ]; then \
		echo "$(SCENARIO_FILE) is not exist!"; exit 1; \
	fi
	@python3 misc/acrn-config/board_config/board_cfg_gen.py --board $(BOARD_FILE) --scenario $(SCENARIO_FILE) || exit $$?
	@python3 misc/acrn-config/scenario_config/scenario_cfg_gen.py --board $(BOARD_FILE) --scenario $(SCENARIO_FILE) || exit $$?
	@echo "Import hypervisor configurations from Config-xmls, configurations in source code are ignored!"
else
cfg_src:
	@echo "Use hypervisor configurations from source code directly."

endif

hypervisor: cfg_src
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) SCENARIO=$(SCENARIO) RELEASE=$(RELEASE) clean
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) SCENARIO=$(SCENARIO) RELEASE=$(RELEASE)
ifeq ($(FIRMWARE),uefi)
	echo "building hypervisor as EFI executable..."
	$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT) SCENARIO=$(SCENARIO) EFI_OBJDIR=$(EFI_OUT)
endif

sbl-hypervisor:
	@mkdir -p $(HV_OUT)-sbl/apl-mrb $(HV_OUT)-sbl/apl-up2
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-mrb BOARD=apl-mrb FIRMWARE=sbl SCENARIO=sdc RELEASE=$(RELEASE) clean
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-mrb BOARD=apl-mrb FIRMWARE=sbl SCENARIO=sdc RELEASE=$(RELEASE)
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-up2 BOARD=apl-up2 FIRMWARE=sbl SCENARIO=sdc RELEASE=$(RELEASE) clean
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-up2 BOARD=apl-up2 FIRMWARE=sbl SCENARIO=sdc RELEASE=$(RELEASE)

	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-isd BOARD=kbl-nuc-i7 FIRMWARE=uefi SCENARIO=industry RELEASE=$(RELEASE) clean
	@mkdir -p $(HV_OUT)-isd
	@echo "CONFIG_INDUSTRY=y" > $(HV_OUT)-isd/.config
	@echo 'CONFIG_BOARD="nuc7i7dnb"' >> $(HV_OUT)-isd/.config
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-isd BOARD=kbl-nuc-i7 FIRMWARE=uefi SCENARIO=industry RELEASE=$(RELEASE) oldconfig
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-isd BOARD=kbl-nuc-i7 FIRMWARE=uefi SCENARIO=industry RELEASE=$(RELEASE)

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

hypervisor-install: cfg_src
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) SCENARIO=$(SCENARIO) RELEASE=$(RELEASE) install
ifeq ($(FIRMWARE),uefi)
	$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) SCENARIO=$(SCENARIO) EFI_OBJDIR=$(EFI_OUT) all install
endif

hypervisor-install-debug: cfg_src
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

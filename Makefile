
# global helper variables
T := $(CURDIR)

BOARD ?= apl-nuc

ifneq (,$(filter $(BOARD),apl-mrb))
	FIRMWARE ?= sbl
else
	FIRMWARE ?= uefi
endif

RELEASE ?= 0

O ?= build
ROOT_OUT := $(shell mkdir -p $(O);cd $(O);pwd)
HV_OUT := $(ROOT_OUT)/hypervisor
EFI_OUT := $(ROOT_OUT)/misc/efi-stub
DM_OUT := $(ROOT_OUT)/devicemodel
TOOLS_OUT := $(ROOT_OUT)/misc/tools
DOC_OUT := $(ROOT_OUT)/doc
BUILD_VERSION ?=
BUILD_TAG ?=
export TOOLS_OUT

.PHONY: all hypervisor devicemodel tools doc
all: hypervisor devicemodel tools

hypervisor:
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) RELEASE=$(RELEASE) clean
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) RELEASE=$(RELEASE)
ifeq ($(FIRMWARE),uefi)
	echo "building hypervisor as EFI executable..."
	$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT) EFI_OBJDIR=$(EFI_OUT)
endif

sbl-hypervisor:
	@mkdir -p $(HV_OUT)-sbl/apl-mrb $(HV_OUT)-sbl/apl-up2
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-mrb BOARD=apl-mrb FIRMWARE=sbl RELEASE=$(RELEASE) clean
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-mrb BOARD=apl-mrb FIRMWARE=sbl RELEASE=$(RELEASE)
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-up2 BOARD=apl-up2 FIRMWARE=sbl RELEASE=$(RELEASE) clean
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-up2 BOARD=apl-up2 FIRMWARE=sbl RELEASE=$(RELEASE)

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
ifeq ($(FIRMWARE),sbl)
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) RELEASE=$(RELEASE) install
endif
ifeq ($(FIRMWARE),uefi)
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) RELEASE=$(RELEASE) install
	$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT) EFI_OBJDIR=$(EFI_OUT) all install
endif

hypervisor-install-debug:
ifeq ($(FIRMWARE),sbl)
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) BOARD=$(BOARD) FIRMWARE=$(FIRMWARE) RELEASE=$(RELEASE) install-debug
endif
ifeq ($(FIRMWARE),uefi)
	$(MAKE) -C $(T)/misc/efi-stub HV_OBJDIR=$(HV_OUT) EFI_OBJDIR=$(EFI_OUT) all install-debug
endif

sbl-hypervisor-install:
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-mrb BOARD=apl-mrb FIRMWARE=sbl RELEASE=$(RELEASE) install
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-up2 BOARD=apl-up2 FIRMWARE=sbl RELEASE=$(RELEASE) install

sbl-hypervisor-install-debug:
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-mrb BOARD=apl-mrb FIRMWARE=sbl RELEASE=$(RELEASE) install-debug
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl/apl-up2 BOARD=apl-up2 FIRMWARE=sbl RELEASE=$(RELEASE) install-debug

devicemodel-install:
	$(MAKE) -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT) install

tools-install:
	$(MAKE) -C $(T)/misc OUT_DIR=$(TOOLS_OUT) RELEASE=$(RELEASE) install

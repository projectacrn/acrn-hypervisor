# acrn-hypervisor/Makefile

# Explicitly set the shell to be used
SHELL := /bin/bash

# global helper variables
T := $(CURDIR)

ifdef TARGET_DIR
  $(warning TARGET_DIR is obsoleted because generated configuration files are now stored in the build directory)
endif

ifdef KCONFIG_FILE
  $(warning KCONFIG_FILE is no longer supported)
  $(error To specify the target board and scenario, define BOARD/SCENARIO variables on the command line)
endif

# BOARD/SCENARIO/BOARD_FILE/SCENARIO_FILE parameters sanity check:
#
# Only below usages are VALID: (target = all | hypervisor)
# 1. make <target>
# 2. make <target> BOARD=xxx SCENARIO=xxx
# 3. make <target> BOARD_FILE=xxx SCENARIO_FILE=xxx
#
# For case 1 that no any parameters are specified, the default BOARD/SCENARIO will be loaded:
#       i.e. equal: make <target> BOARD=kbl-nuc-i7 SCENARIO=industry
#
# For case 2/3, configurations are from XML files and saved to HV_OUT

ifneq ($(BOARD)$(SCENARIO),)
  ifneq ($(BOARD_FILE)$(SCENARIO_FILE),)
    $(error BOARD/SCENARIO parameter could not coexist with BOARD_FILE/SCENARIO_FILE)
  endif
endif

ifneq ($(BOARD_FILE)$(SCENARIO_FILE),)
  ifneq ($(BOARD_FILE), $(wildcard $(BOARD_FILE)))
    $(error BOARD_FILE: $(BOARD_FILE) does not exist)
  endif
  ifneq ($(SCENARIO_FILE), $(wildcard $(SCENARIO_FILE)))
    $(error SCENARIO_FILE: $(SCENARIO_FILE) does not exist)
  endif

  override BOARD := $(realpath $(BOARD_FILE))
  override SCENARIO := $(realpath $(SCENARIO_FILE))
else
  # BOARD/SCENARIO pointing to XML files must be converted to absolute paths before being passed to hypervisor/Makefile
  # because paths relative to acrn-hypervisor/ are typically invalid when relative to acrn-hypervisor/Makefile
  ifneq ($(realpath $(BOARD)),)
    override BOARD := $(realpath $(BOARD))
  endif
  ifneq ($(realpath $(SCENARIO)),)
    override SCENARIO := $(realpath $(SCENARIO))
  endif
endif

# Backward-compatibility for RELEASE=(0|1)
ifeq ($(RELEASE),1)
  override RELEASE := y
else
  ifeq ($(RELEASE),0)
    override RELEASE := n
  endif
endif

O ?= build
ROOT_OUT := $(shell mkdir -p $(O);cd $(O);pwd)
HV_OUT := $(ROOT_OUT)/hypervisor
DM_OUT := $(ROOT_OUT)/devicemodel
TOOLS_OUT := $(ROOT_OUT)/misc
DOC_OUT := $(ROOT_OUT)/doc
BUILD_VERSION ?=
BUILD_TAG ?=
HV_CFG_LOG = $(HV_OUT)/cfg.log
VM_CONFIGS_DIR = $(T)/misc/config_tools
ASL_COMPILER ?= $(shell which iasl)

.PHONY: all hypervisor devicemodel tools life_mngr doc
all: hypervisor devicemodel tools
	@cat $(HV_CFG_LOG)

#help functions to build acrn and install acrn/acrn symbols
define build_acrn
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)/$(1) clean
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)/$(1) BOARD=$(1) SCENARIO=$(2) RELEASE=$(RELEASE)
endef

define install_acrn
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)/$(1) BOARD=$(1) SCENARIO=$(2) RELEASE=$(RELEASE) install
endef

define install_acrn_debug
	$(MAKE) -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)/$(1) BOARD=$(1) SCENARIO=$(2) RELEASE=$(RELEASE) install-debug
endef

HV_MAKEOPTS := -C $(T)/hypervisor BOARD=$(BOARD) SCENARIO=$(SCENARIO) HV_OBJDIR=$(HV_OUT) RELEASE=$(RELEASE)

hypervisor: hvdefconfig
	$(MAKE) $(HV_MAKEOPTS)
	@echo -e "ACRN Configuration Summary:" > $(HV_CFG_LOG)
	@$(MAKE) showconfig $(HV_MAKEOPTS) -s >> $(HV_CFG_LOG)
	@cat $(HV_CFG_LOG)

# Targets that manipulate hypervisor configurations
hvdefconfig:
	@$(MAKE) defconfig $(HV_MAKEOPTS)

hvshowconfig:
	@$(MAKE) showconfig $(HV_MAKEOPTS) -s

hvdiffconfig:
	@$(MAKE) diffconfig $(HV_MAKEOPTS)

hvapplydiffconfig:
	@$(MAKE) applydiffconfig $(HV_MAKEOPTS) PATCH=$(abspath $(PATCH))

devicemodel: tools
	$(MAKE) -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT) DM_BUILD_VERSION=$(BUILD_VERSION) DM_BUILD_TAG=$(BUILD_TAG) DM_ASL_COMPILER=$(ASL_COMPILER) TOOLS_OUT=$(TOOLS_OUT) RELEASE=$(RELEASE)

tools:
	mkdir -p $(TOOLS_OUT)
	$(MAKE) -C $(T)/misc OUT_DIR=$(TOOLS_OUT) RELEASE=$(RELEASE)

life_mngr:
	mkdir -p $(TOOLS_OUT)
	$(MAKE) -C $(T)/misc OUT_DIR=$(TOOLS_OUT) RELEASE=$(RELEASE) life_mngr

doc:
	$(MAKE) -C $(T)/doc html BUILDDIR=$(DOC_OUT)

.PHONY: clean
clean:
	$(MAKE) -C $(T)/misc OUT_DIR=$(TOOLS_OUT) clean
	$(MAKE) -C $(T)/doc BUILDDIR=$(DOC_OUT) clean
	rm -rf $(ROOT_OUT)

.PHONY: install life_mngr-install
install: hypervisor-install devicemodel-install tools-install

hypervisor-install:
	$(MAKE) $(HV_MAKEOPTS) install

hypervisor-install-debug:
	$(MAKE) $(HV_MAKEOPTS) install-debug

kbl-nuc-i7-industry:
	$(call build_acrn,nuc7i7dnb,industry)
apl-up2-hybrid:
	$(call build_acrn,apl-up2,hybrid)

sbl-hypervisor: kbl-nuc-i7-industry \
                apl-up2-hybrid

kbl-nuc-i7-industry-install:
	$(call install_acrn,nuc7i7dnb,industry)
apl-up2-hybrid-install:
	$(call install_acrn,apl-up2,hybrid)

sbl-hypervisor-install: kbl-nuc-i7-industry-install \
                        apl-up2-hybrid-install

kbl-nuc-i7-industry-install-debug:
	$(call install_acrn_debug,nuc7i7dnb,industry)
apl-up2-hybrid-install-debug:
	$(call install_acrn_debug,apl-up2,hybrid)

sbl-hypervisor-install-debug: kbl-nuc-i7-industry-install-debug \
			      apl-up2-hybrid-install-debug

devicemodel-install:
	$(MAKE) -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT) install

tools-install:
	$(MAKE) -C $(T)/misc OUT_DIR=$(TOOLS_OUT) RELEASE=$(RELEASE) install

life_mngr-install:
	$(MAKE) -C $(T)/misc OUT_DIR=$(TOOLS_OUT) RELEASE=$(RELEASE) acrn-life-mngr-install

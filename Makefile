# acrn-hypervisor/Makefile

# Explicitly set the shell to be used
SHELL := /bin/bash

# global helper variables
T := $(CURDIR)

# ACRN Version Information
include VERSION
SCM_VERSION := $(shell [ -d .git ] && git describe --exact-match 1>/dev/null 2>&1 || git describe --dirty)
ifneq ($(SCM_VERSION),)
	SCM_VERSION := "-"$(SCM_VERSION)
endif
export FULL_VERSION=$(MAJOR_VERSION).$(MINOR_VERSION)$(EXTRA_VERSION)$(SCM_VERSION)
ROMOTE_BRANCH := $(shell [ -d .git ] && git rev-parse --abbrev-ref HEAD)
STABLE_STR := -stable
ifeq ($(EXTRA_VERSION), -unstable)
	STABLE_STR := -unstable
endif
export BRANCH_VERSION=$(ROMOTE_BRANCH)$(STABLE_STR)

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
  override SCENARIO := $(abspath $(SCENARIO_FILE))
else
  # BOARD/SCENARIO pointing to XML files must be converted to absolute paths before being passed to hypervisor/Makefile
  # because paths relative to acrn-hypervisor/ are typically invalid when relative to acrn-hypervisor/Makefile
  ifneq ($(realpath $(BOARD)),)
    override BOARD := $(realpath $(BOARD))
  endif
  ifneq ($(realpath $(SCENARIO)),)
    override SCENARIO := $(abspath $(SCENARIO))
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
TARBALL_OUT := $(ROOT_OUT)/tarball
BUILD_VERSION ?=
BUILD_TAG ?=
HV_CFG_LOG = $(HV_OUT)/cfg.log
VM_CONFIGS_DIR = $(T)/misc/config_tools
ASL_COMPILER ?= $(shell which iasl)
DPKG_BIN ?= $(shell which dpkg)
YARN_BIN ?= $(shell which yarn)
CARGO_BIN ?= $(shell which cargo)
IASL_MIN_VER = "20190703"

.PHONY: all hypervisor devicemodel tools life_mngr doc
all: hypervisor devicemodel tools
	@cat $(HV_CFG_LOG)
	@if [ -x "$(DPKG_BIN)" ]; then \
	  DEB_BOARD=$$(grep "BOARD" $(HV_CFG_LOG) | awk -F '= ' '{print  $$2 }'); \
	  DEB_SCENARIO=$$(grep "SCENARIO" $(HV_CFG_LOG) | awk -F '= ' '{print  $$2 }'); \
	  python3 misc/packaging/gen_acrn_deb.py acrn_all $(ROOT_OUT) --version=$(FULL_VERSION) --board_name="$$DEB_BOARD" --scenario="$$DEB_SCENARIO"; \
	fi

HV_MAKEOPTS := -C $(T)/hypervisor BOARD=$(BOARD) SCENARIO=$(SCENARIO) HV_OBJDIR=$(HV_OUT) RELEASE=$(RELEASE) ASL_COMPILER=$(ASL_COMPILER) IASL_MIN_VER=$(IASL_MIN_VER)

board_inspector:
	@if [ -x "$(DPKG_BIN)" ]; then \
	  python3 misc/packaging/gen_acrn_deb.py board_inspector $(ROOT_OUT) --version=$(FULL_VERSION); \
	else \
	  echo -e "The 'dpkg' utility is not available. Unable to create Debian package for board_inspector."; \
	fi

configurator:
	@if [ -x "$(YARN_BIN)" ] && [ -x "$(CARGO_BIN)" ]; then \
	  python3 misc/packaging/gen_acrn_deb.py configurator $(ROOT_OUT) --version=$(FULL_VERSION); \
	else \
	  echo -e "'yarn' or 'cargo' utility is not available. Unable to create Debian package for configurator."; \
	fi

hypervisor:
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
	$(MAKE) -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT) DM_BUILD_VERSION=$(BUILD_VERSION) DM_BUILD_TAG=$(BUILD_TAG) TOOLS_OUT=$(TOOLS_OUT) RELEASE=$(RELEASE) IASL_MIN_VER=$(IASL_MIN_VER)

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
	python3 misc/packaging/gen_acrn_deb.py clean $(ROOT_OUT) --version=$(FULL_VERSION);

.PHONY: install life_mngr-install
install: hypervisor-install devicemodel-install tools-install

hypervisor-install: hypervisor
	$(MAKE) $(HV_MAKEOPTS) install

hypervisor-install-debug:
	$(MAKE) $(HV_MAKEOPTS) install-debug

devicemodel-install: tools-install devicemodel
	$(MAKE) -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT) install

tools-install: tools
	$(MAKE) -C $(T)/misc OUT_DIR=$(TOOLS_OUT) RELEASE=$(RELEASE) install

life_mngr-install:
	$(MAKE) -C $(T)/misc OUT_DIR=$(TOOLS_OUT) RELEASE=$(RELEASE) acrn-life-mngr-install

.PHONY: targz-pkg
targz-pkg:
	$(MAKE) install DESTDIR=$(TARBALL_OUT)
	cd $(TARBALL_OUT) && \
	tar -zcvf $(ROOT_OUT)/acrn-$(FULL_VERSION).tar.gz *


# global helper variables
T := $(CURDIR)
PLATFORM ?= uefi
RELEASE ?= 0

O ?= build
ROOT_OUT := $(shell mkdir -p $(O);cd $(O);pwd)
HV_OUT := $(ROOT_OUT)/hypervisor
DM_OUT := $(ROOT_OUT)/devicemodel
TOOLS_OUT := $(ROOT_OUT)/tools
DOC_OUT := $(ROOT_OUT)/doc
BUILD_VERSION ?=
BUILD_TAG ?=
export TOOLS_OUT

.PHONY: all hypervisor devicemodel tools doc
all: hypervisor devicemodel tools

hypervisor:
	make -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) PLATFORM=$(PLATFORM) RELEASE=$(RELEASE) clean
	make -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) PLATFORM=$(PLATFORM) RELEASE=$(RELEASE)

sbl-hypervisor:
	@mkdir -p $(HV_OUT)-sbl
	make -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl PLATFORM=sbl RELEASE=$(RELEASE) clean
	make -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl PLATFORM=sbl RELEASE=$(RELEASE)

devicemodel: tools
	make -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT) clean
	make -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT) DM_BUILD_VERSION=$(BUILD_VERSION) DM_BUILD_TAG=$(BUILD_TAG)

tools:
	mkdir -p $(TOOLS_OUT)
	make -C $(T)/tools OUT_DIR=$(TOOLS_OUT) RELEASE=$(RELEASE)

doc:
	make -C $(T)/doc html BUILDDIR=$(DOC_OUT)

.PHONY: clean
clean:
	make -C $(T)/tools OUT_DIR=$(TOOLS_OUT) clean
	make -C $(T)/doc BUILDDIR=$(DOC_OUT) clean
	rm -rf $(ROOT_OUT)

.PHONY: install
install: hypervisor-install devicemodel-install tools-install

hypervisor-install:
	make -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) PLATFORM=$(PLATFORM) RELEASE=$(RELEASE) install

sbl-hypervisor-install:
	make -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl PLATFORM=sbl RELEASE=$(RELEASE) install

devicemodel-install:
	make -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT) install

tools-install:
	make -C $(T)/tools OUT_DIR=$(TOOLS_OUT) install

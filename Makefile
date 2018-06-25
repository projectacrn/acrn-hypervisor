
# global helper variables
T := $(CURDIR)
PLATFORM ?= uefi
RELEASE ?= 0

O ?= build
ROOT_OUT := $(shell mkdir -p $(O);cd $(O);pwd)
HV_OUT := $(ROOT_OUT)/hypervisor
DM_OUT := $(ROOT_OUT)/devicemodel
TOOLS_OUT := $(ROOT_OUT)/tools
MISC_OUT := $(ROOT_OUT)/misc
DOC_OUT := $(ROOT_OUT)/doc
export TOOLS_OUT

.PHONY: all hypervisor devicemodel tools misc doc
all: hypervisor devicemodel tools misc

hypervisor:
	make -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) PLATFORM=$(PLATFORM) RELEASE=$(RELEASE) clean
	make -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) PLATFORM=$(PLATFORM) RELEASE=$(RELEASE)

sbl-hypervisor:
	@mkdir -p $(HV_OUT)-sbl
	make -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl PLATFORM=sbl RELEASE=$(RELEASE) clean
	make -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl PLATFORM=sbl RELEASE=$(RELEASE)

devicemodel: tools
	make -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT) clean
	make -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT)

tools:
	mkdir -p $(TOOLS_OUT)
	make -C $(T)/tools OUT_DIR=$(TOOLS_OUT) RELEASE=$(RELEASE)

misc: tools
	mkdir -p $(MISC_OUT)
	make -C $(T)/misc OUT_DIR=$(MISC_OUT)

doc:
	make -C $(T)/doc html BUILDDIR=$(DOC_OUT)

.PHONY: clean
clean:
	make -C $(T)/tools OUT_DIR=$(TOOLS_OUT) clean
	make -C $(T)/doc BUILDDIR=$(DOC_OUT) clean
	rm -rf $(ROOT_OUT)

.PHONY: install
install: hypervisor-install devicemodel-install tools-install misc-install

hypervisor-install:
	make -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) PLATFORM=$(PLATFORM) RELEASE=$(RELEASE) install

sbl-hypervisor-install:
	make -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT)-sbl PLATFORM=sbl RELEASE=$(RELEASE) install

devicemodel-install:
	make -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT) install

tools-install:
	make -C $(T)/tools OUT_DIR=$(TOOLS_OUT) install

misc-install:
	make -C $(T)/misc OUT_DIR=$(MISC_OUT) install

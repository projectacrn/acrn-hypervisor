
# global helper variables
T := $(CURDIR)
PLATFORM ?= uefi
RELEASE ?= 0

O ?= build
ROOT_OUT := $(shell mkdir -p $(O);cd $(O);pwd)
HV_OUT := $(ROOT_OUT)/hypervisor
DM_OUT := $(ROOT_OUT)/devicemodel
TOOLS_OUT := $(ROOT_OUT)/tools

.PHONY: all hypervisor devicemodel tools
all: hypervisor devicemodel tools

hypervisor:
	make -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) PLATFORM=$(PLATFORM) RELEASE=$(RELEASE) clean
	make -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) PLATFORM=$(PLATFORM) RELEASE=$(RELEASE)

devicemodel:
	make -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT) clean
	make -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT)

tools:
	mkdir -p $(TOOLS_OUT)
	make -C $(T)/tools/acrnlog OUT_DIR=$(TOOLS_OUT)
	make -C $(T)/tools/acrn-manager OUT_DIR=$(TOOLS_OUT)
	make -C $(T)/tools/acrntrace OUT_DIR=$(TOOLS_OUT)
	make -C $(T)/tools/acrn-crashlog OUT_DIR=$(TOOLS_OUT) RELEASE=$(RELEASE)

.PHONY: clean
clean:
	rm -rf $(ROOT_OUT)

.PHONY: install
install: hypervisor-install devicemodel-install tools-install

hypervisor-install:
	make -C $(T)/hypervisor HV_OBJDIR=$(HV_OUT) PLATFORM=$(PLATFORM) RELEASE=$(RELEASE) install

devicemodel-install:
	make -C $(T)/devicemodel DM_OBJDIR=$(DM_OUT) install

tools-install:
	make -C $(T)/tools/acrnlog OUT_DIR=$(TOOLS_OUT) install
	make -C $(T)/tools/acrn-manager OUT_DIR=$(TOOLS_OUT) install
	make -C $(T)/tools/acrntrace OUT_DIR=$(TOOLS_OUT) install
	make -C $(T)/tools/acrn-crashlog OUT_DIR=$(TOOLS_OUT) install

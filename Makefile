
# global helper variables
T := $(CURDIR)
PLAT := uefi
RELEASE := 0

ROOT_OUT := $(T)/build
HV_OUT := $(ROOT_OUT)/hypervisor
DM_OUT := $(ROOT_OUT)/devicemodel
TOOLS_OUT := $(ROOT_OUT)/tools

.PHONY: all hypervisor devicemodel tools
all: hypervisor devicemodel tools

hypervisor:
	cd $(T)/hypervisor; \
	make HV_OBJDIR=$(HV_OUT) PLATFORM=$(PLAT) RELEASE=$(RELEASE) clean; \
	make HV_OBJDIR=$(HV_OUT) PLATFORM=$(PLAT) RELEASE=$(RELEASE)

devicemodel:
	cd $(T)/devicemodel; \
	make DM_OBJDIR=$(DM_OUT) clean; \
	make DM_OBJDIR=$(DM_OUT)

tools:
	mkdir -p $(TOOLS_OUT)
	cd $(T)/tools/acrnlog; \
	make OUT_DIR=$(TOOLS_OUT);
	cd $(T)/tools/acrn-manager; \
	make OUT_DIR=$(TOOLS_OUT);
	cd $(T)/tools/acrntrace; \
	make OUT_DIR=$(TOOLS_OUT);

.PHONY: clean
clean:
	rm -rf $(ROOT_OUT)
	



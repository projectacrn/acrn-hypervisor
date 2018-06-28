
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

# With V=1 means verbose build. Will show full build commands
#
# Without V means silient build. Only show customed message. And we
# could put more focous on warning.
ifdef V
  Q :=
  vecho := @true
  SUBOPT := "V=1"
else
  Q := @
  vecho := @echo
  SUBOPT:= "--no-print-directory"
endif

.PHONY: all hypervisor devicemodel tools misc doc
all: hypervisor devicemodel tools misc

hypervisor:
	$(vecho) "Build hypervisor"
	$(Q)make -C $(T)/hypervisor $(SUBOPT) HV_OBJDIR=$(HV_OUT) PLATFORM=$(PLATFORM) RELEASE=$(RELEASE) clean
	$(Q)make -C $(T)/hypervisor $(SUBOPT) HV_OBJDIR=$(HV_OUT) PLATFORM=$(PLATFORM) RELEASE=$(RELEASE)

sbl-hypervisor:
	$(vecho) "Build sbl-hypervisor"
	@mkdir -p $(HV_OUT)-sbl
	$(Q)make -C $(T)/hypervisor $(SUBOPT) HV_OBJDIR=$(HV_OUT)-sbl PLATFORM=sbl RELEASE=$(RELEASE) clean
	$(Q)make -C $(T)/hypervisor $(SUBOPT) HV_OBJDIR=$(HV_OUT)-sbl PLATFORM=sbl RELEASE=$(RELEASE)

devicemodel: tools
	$(vecho) "Build devicemodel"
	$(Q)make -C $(T)/devicemodel $(SUBOPT) DM_OBJDIR=$(DM_OUT) clean
	$(Q)make -C $(T)/devicemodel $(SUBOPT) DM_OBJDIR=$(DM_OUT)

tools:
	$(vecho) "Build tools"
	$(Q)mkdir -p $(TOOLS_OUT)
	$(Q)make -C $(T)/tools $(SUBOPT) OUT_DIR=$(TOOLS_OUT) RELEASE=$(RELEASE)

misc: tools
	$(vecho) "Build misc"
	$(Q)mkdir -p $(MISC_OUT)
	$(Q)make -C $(T)/misc $(SUBOPT) OUT_DIR=$(MISC_OUT)

doc:
	$(vecho) "Build Documents"
	$(Q)make -C $(T)/doc $(SUBOPT) html BUILDDIR=$(DOC_OUT)

.PHONY: clean
clean:
	$(vecho) "Cleanup"
	$(Q)make -C $(T)/tools $(SUBOPT) OUT_DIR=$(TOOLS_OUT) clean
	$(Q)make -C $(T)/doc $(SUBOPT) BUILDDIR=$(DOC_OUT) clean
	$(Q)rm -rf $(ROOT_OUT)

.PHONY: install
install: hypervisor-install devicemodel-install tools-install misc-install

hypervisor-install:
	$(vecho) "Install hypervisor"
	$(Q)make -C $(T)/hypervisor $(SUBOPT) HV_OBJDIR=$(HV_OUT) PLATFORM=$(PLATFORM) RELEASE=$(RELEASE) install

sbl-hypervisor-install:
	$(vecho) "Install sbl-hypervisor"
	$(Q)make -C $(T)/hypervisor $(SUBOPT) HV_OBJDIR=$(HV_OUT)-sbl PLATFORM=sbl RELEASE=$(RELEASE) install

devicemodel-install:
	$(vecho) "Install devicemodel"
	$(Q)make -C $(T)/devicemodel $(SUBOPT) DM_OBJDIR=$(DM_OUT) install

tools-install:
	$(vecho) "Install tools"
	$(Q)make -C $(T)/tools $(SUBOPT) OUT_DIR=$(TOOLS_OUT) install

misc-install:
	$(vecho) "Install misc"
	$(Q)make -C $(T)/misc $(SUBOPT) OUT_DIR=$(MISC_OUT) install

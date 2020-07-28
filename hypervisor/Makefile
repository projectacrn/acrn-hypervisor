#
# acrn-hypervisor/hypervisor/Makefile
#

include ../VERSION
FULL_VERSION=$(MAJOR_VERSION).$(MINOR_VERSION)$(EXTRA_VERSION)

API_MAJOR_VERSION=1
API_MINOR_VERSION=0

GCC_MAJOR=$(shell echo __GNUC__ | $(CC) -E -x c - | tail -n 1)
GCC_MINOR=$(shell echo __GNUC_MINOR__ | $(CC) -E -x c - | tail -n 1)

#enable stack overflow check
STACK_PROTECTOR := 1

BASEDIR := $(shell pwd)
HV_OBJDIR ?= $(CURDIR)/build
HV_MODDIR ?= $(HV_OBJDIR)/modules
HV_FILE := acrn

ifeq ($(TARGET_DIR),)
  override TARGET_DIR := $(realpath ../misc/vm_configs)
endif

LIB_MOD = $(HV_MODDIR)/lib_mod.a
BOOT_MOD = $(HV_MODDIR)/boot_mod.a
HW_MOD = $(HV_MODDIR)/hw_mod.a
VP_BASE_MOD = $(HV_MODDIR)/vp_base_mod.a
VP_DM_MOD = $(HV_MODDIR)/vp_dm_mod.a
VP_TRUSTY_MOD = $(HV_MODDIR)/vp_trusty_mod.a
VP_HCALL_MOD = $(HV_MODDIR)/vp_hcall_mod.a
LIB_DEBUG = $(HV_MODDIR)/libdebug.a
LIB_RELEASE = $(HV_MODDIR)/librelease.a
SYS_INIT_MOD = $(HV_MODDIR)/sys_init_mod.a

# initialize the flags we used
CFLAGS :=
ASFLAGS :=
LDFLAGS :=
ARFLAGS :=
ARCH_CFLAGS :=
ARCH_ASFLAGS :=
ARCH_ARFLAGS :=
ARCH_LDFLAGS :=

.PHONY: default
default: all

include scripts/makefile/cfg_update.mk

include scripts/makefile/deps.mk

include scripts/makefile/kconfig.mk

#override BOARD from CONFIG_BOARD, which is specified in kconfig.mk
override BOARD := $(shell echo $(CONFIG_BOARD) |sed 's/"//g')

#override SCENARIO from CONFIG_SCENARIO, which is specified in kconfig.mk
override SCENARIO := $(shell echo $(CONFIG_SCENARIO) |sed 's/"//g')

BOARD_INFO_DIR := $(TARGET_DIR)/boards/$(BOARD)
SCENARIO_CFG_DIR := $(TARGET_DIR)/scenarios/$(SCENARIO)
BOARD_CFG_DIR := $(SCENARIO_CFG_DIR)/$(BOARD)

include ../paths.make

LD_IN_TOOL = scripts/genld.sh
BASH = $(shell which bash)

ARFLAGS += crs

CFLAGS += -Wall -W
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -fshort-wchar -ffreestanding
CFLAGS += -fsigned-char
CFLAGS += -m64 -mno-mmx -mno-sse -mno-sse2 -mno-80387 -mno-fp-ret-in-387
CFLAGS += -mno-red-zone -mpopcnt
CFLAGS += -nostdinc -nostdlib -fno-common
CFLAGS += -Werror

# ACRN depends on zero length array. Silence the gcc if Warrary-bounds is default option
CFLAGS += -Wno-array-bounds
CFLAGS += -O2
ifeq (y, $(CONFIG_RELOC))
CFLAGS += -fpie
else
CFLAGS += -static
endif


ifdef STACK_PROTECTOR
ifeq (true, $(shell [ $(GCC_MAJOR) -gt 4 ] && echo true))
CFLAGS += -fstack-protector-strong
else
ifeq (true, $(shell [ $(GCC_MAJOR) -eq 4 ] && [ $(GCC_MINOR) -ge 9 ] && echo true))
CFLAGS += -fstack-protector-strong
else
CFLAGS += -fstack-protector
endif
endif
CFLAGS += -DSTACK_PROTECTOR
endif

# In case using Ubuntu 19.10 as build environment, its gcc has -fcf-protection enabled
# by default. But this option is not compatible with -mindirect-branch:
#   https://bugs.launchpad.net/ubuntu/+source/gcc-9/+bug/1830961
#
# We disable the -fcf-protection in ACRN build.
ifeq (true, $(shell [ $(GCC_MAJOR) -gt 8 ] && echo true))
CFLAGS += -fcf-protection=none
endif

ASFLAGS += -m64 -nostdinc -nostdlib
ifeq (y, $(CONFIG_MULTIBOOT2))
ASFLAGS += -DCONFIG_MULTIBOOT2
endif

ifeq (y, $(CONFIG_RELOC))
ASFLAGS += -DCONFIG_RELOC
endif

LDFLAGS += -Wl,--gc-sections -nostartfiles -nostdlib
LDFLAGS += -Wl,-n,-z,max-page-size=0x1000
LDFLAGS += -Wl,--no-dynamic-linker

ifeq (y, $(CONFIG_RELOC))
# on X86_64, when build with "-pie", GCC fails on linking R_X86_64_32
# relocations with "recompile with fPIC" error, because it may cause
# run-time relocation overflow if it runs at address above 4GB.
# We know it's safe because Hypervisor runs under 4GB. "noreloc-overflow"
# is used to avoid the compile error
LDFLAGS += -pie -z noreloc-overflow
else
LDFLAGS += -static
endif

ifeq (y, $(CONFIG_RELEASE))
LDFLAGS += -s
endif

ARCH_CFLAGS += -gdwarf-2
ARCH_ASFLAGS += -gdwarf-2 -DASSEMBLER=1
ARCH_ARFLAGS +=
ARCH_LDFLAGS +=

ARCH_LDSCRIPT = $(HV_OBJDIR)/link_ram.ld
ARCH_LDSCRIPT_IN = bsp/ld/link_ram.ld.in

INCLUDE_PATH += include
INCLUDE_PATH += include/lib
INCLUDE_PATH += include/lib/crypto
INCLUDE_PATH += include/common
INCLUDE_PATH += include/arch/x86
INCLUDE_PATH += include/arch/x86/boot
INCLUDE_PATH += include/arch/x86/guest
INCLUDE_PATH += include/arch/x86/lib
INCLUDE_PATH += include/debug
INCLUDE_PATH += include/public
INCLUDE_PATH += include/dm
INCLUDE_PATH += include/hw
INCLUDE_PATH += boot/include
INCLUDE_PATH += boot/include/guest
INCLUDE_PATH += $(HV_OBJDIR)/include
INCLUDE_PATH += $(BOARD_INFO_DIR)
INCLUDE_PATH += $(BOARD_CFG_DIR)
INCLUDE_PATH += $(SCENARIO_CFG_DIR)

override INCLUDE_PATH := $(realpath $(INCLUDE_PATH))

CC ?= gcc
AS ?= as
AR ?= ar
LD ?= ld
OBJCOPY ?= objcopy

export CC AS AR LD OBJCOPY
export CFLAGS ASFLAGS ARFLAGS LDFLAGS ARCH_CFLAGS ARCH_ASFLAGS ARCH_ARFLAGS ARCH_LDFLAGS
export HV_OBJDIR HV_MODDIR CONFIG_RELEASE INCLUDE_PATH
export LIB_DEBUG LIB_RELEASE

# library componment
LIB_C_SRCS += lib/string.c
LIB_C_SRCS += lib/crypto/crypto_api.c
LIB_C_SRCS += lib/crypto/mbedtls/hkdf.c
LIB_C_SRCS += lib/crypto/mbedtls/sha256.c
LIB_C_SRCS += lib/crypto/mbedtls/md.c
LIB_C_SRCS += lib/crypto/mbedtls/md_wrap.c
LIB_C_SRCS += lib/sprintf.c
LIB_C_SRCS += arch/x86/lib/memory.c
ifdef STACK_PROTECTOR
LIB_C_SRCS += lib/stack_protector.c
endif
# retpoline support
ifeq (true, $(shell [ $(GCC_MAJOR) -eq 7 ] && [ $(GCC_MINOR) -ge 3 ] && echo true))
CFLAGS += -mindirect-branch=thunk-extern -mindirect-branch-register
CFLAGS += -DCONFIG_RETPOLINE
LIB_S_SRCS += arch/x86/lib/retpoline-thunk.S
else
ifeq (true, $(shell [ $(GCC_MAJOR) -ge 8 ] && echo true))
CFLAGS += -mindirect-branch=thunk-extern -mindirect-branch-register
CFLAGS += -DCONFIG_RETPOLINE
LIB_S_SRCS += arch/x86/lib/retpoline-thunk.S
endif
endif

# platform boot component
BOOT_S_SRCS += arch/x86/boot/cpu_primary.S
BOOT_S_SRCS += arch/x86/boot/cpu_save_boot_ctx.S
BOOT_S_SRCS += arch/x86/boot/trampoline.S
BOOT_C_SRCS += boot/multiboot.c
ifeq ($(CONFIG_MULTIBOOT2),y)
BOOT_C_SRCS += boot/multiboot2.c
endif
BOOT_C_SRCS += boot/reloc.c

# hardware management component
HW_S_SRCS += arch/x86/idt.S
HW_C_SRCS += arch/x86/ioapic.c
HW_C_SRCS += arch/x86/lapic.c
HW_C_SRCS += arch/x86/cpu.c
HW_C_SRCS += arch/x86/cpu_caps.c
HW_C_SRCS += arch/x86/platform_caps.c
HW_C_SRCS += arch/x86/security.c
HW_C_SRCS += arch/x86/mmu.c
HW_C_SRCS += arch/x86/e820.c
HW_C_SRCS += arch/x86/pagetable.c
HW_C_SRCS += arch/x86/page.c
HW_C_SRCS += arch/x86/notify.c
HW_C_SRCS += arch/x86/vtd.c
HW_C_SRCS += arch/x86/gdt.c
HW_C_SRCS += arch/x86/irq.c
HW_C_SRCS += arch/x86/timer.c
HW_C_SRCS += arch/x86/vmx.c
HW_C_SRCS += arch/x86/cpu_state_tbl.c
HW_C_SRCS += arch/x86/pm.c
HW_S_SRCS += arch/x86/wakeup.S
HW_C_SRCS += arch/x86/trampoline.c
HW_S_SRCS += arch/x86/sched.S
HW_C_SRCS += arch/x86/rdt.c
HW_C_SRCS += arch/x86/sgx.c
HW_C_SRCS += common/softirq.c
HW_C_SRCS += common/schedule.c
HW_C_SRCS += common/event.c
ifeq ($(CONFIG_SCHED_NOOP),y)
HW_C_SRCS += common/sched_noop.c
endif
ifeq ($(CONFIG_SCHED_IORR),y)
HW_C_SRCS += common/sched_iorr.c
endif
ifeq ($(CONFIG_SCHED_BVT),y)
HW_C_SRCS += common/sched_bvt.c
endif
HW_C_SRCS += hw/pci.c
HW_C_SRCS += arch/x86/configs/vm_config.c
HW_C_SRCS += boot/acpi_base.c
HW_C_SRCS += boot/cmdline.c
# ACPI parsing component
# This part should be isolated from FuSa Cert
ifeq ($(CONFIG_ACPI_PARSE_ENABLED),y)
HW_C_SRCS += acpi_parser/dmar_parse.c
HW_C_SRCS += acpi_parser/acpi_ext.c
endif
HW_C_SRCS += boot/guest/vboot_wrapper.c
HW_C_SRCS += boot/guest/deprivilege_boot.c
HW_C_SRCS += boot/guest/direct_boot.c

# VM Configuration
VM_CFG_C_SRCS += $(BOARD_INFO_DIR)/board.c
VM_CFG_C_SRCS += $(SCENARIO_CFG_DIR)/vm_configurations.c
ifneq (,$(wildcard $(BOARD_CFG_DIR)/pci_dev.c))
VM_CFG_C_SRCS += $(BOARD_CFG_DIR)/pci_dev.c
endif

# virtual platform base component
VP_BASE_C_SRCS += arch/x86/guest/vcpuid.c
VP_BASE_C_SRCS += arch/x86/guest/vcpu.c
VP_BASE_C_SRCS += arch/x86/guest/vm.c
VP_BASE_C_SRCS += arch/x86/guest/vmtrr.c
VP_BASE_C_SRCS += arch/x86/guest/guest_memory.c
VP_BASE_C_SRCS += arch/x86/guest/vmsr.c
VP_BASE_S_SRCS += arch/x86/guest/vmx_asm.S
VP_BASE_C_SRCS += arch/x86/guest/vmcs.c
VP_BASE_C_SRCS += arch/x86/guest/virq.c
VP_BASE_C_SRCS += arch/x86/guest/virtual_cr.c
VP_BASE_C_SRCS += arch/x86/guest/vmexit.c
VP_BASE_C_SRCS += arch/x86/guest/ept.c
VP_BASE_C_SRCS += arch/x86/guest/ve820.c
VP_BASE_C_SRCS += arch/x86/guest/ucode.c
ifeq ($(CONFIG_HYPERV_ENABLED),y)
VP_BASE_C_SRCS += arch/x86/guest/hyperv.c
endif
VP_BASE_C_SRCS += boot/guest/vboot_info.c
VP_BASE_C_SRCS += common/hv_main.c
VP_BASE_C_SRCS += common/vm_load.c
VP_BASE_C_SRCS += arch/x86/configs/pci_dev.c
VP_BASE_C_SRCS += arch/x86/configs/vacpi.c

# virtual platform device model
VP_DM_C_SRCS += dm/vpic.c
VP_DM_C_SRCS += dm/vrtc.c
VP_DM_C_SRCS += dm/vioapic.c
VP_DM_C_SRCS += dm/vuart.c
VP_DM_C_SRCS += dm/io_req.c
VP_DM_C_SRCS += dm/vpci/vdev.c
VP_DM_C_SRCS += dm/vpci/vpci.c
VP_DM_C_SRCS += dm/vpci/vhostbridge.c
VP_DM_C_SRCS += dm/vpci/vpci_bridge.c
VP_DM_C_SRCS += dm/vpci/ivshmem.c
VP_DM_C_SRCS += dm/vpci/pci_pt.c
VP_DM_C_SRCS += dm/vpci/vmsi.c
VP_DM_C_SRCS += dm/vpci/vmsix.c
VP_DM_C_SRCS += dm/vpci/vmsix_on_msi.c
VP_DM_C_SRCS += dm/vpci/vsriov.c
VP_DM_C_SRCS += dm/mmio_dev.c
VP_DM_C_SRCS += arch/x86/guest/vlapic.c
VP_DM_C_SRCS += arch/x86/guest/pm.c
VP_DM_C_SRCS += arch/x86/guest/assign.c
VP_DM_C_SRCS += arch/x86/guest/vmx_io.c
VP_DM_C_SRCS += arch/x86/guest/instr_emul.c
VP_DM_C_SRCS += arch/x86/guest/vm_reset.c
VP_DM_C_SRCS += common/ptdev.c

# virtual platform trusty
VP_TRUSTY_C_SRCS += arch/x86/guest/trusty.c
VP_TRUSTY_C_SRCS += common/trusty_hypercall.c
VP_TRUSTY_C_SRCS += arch/x86/seed/seed.c
VP_TRUSTY_C_SRCS += arch/x86/seed/seed_abl.c
VP_TRUSTY_C_SRCS += arch/x86/seed/seed_sbl.c

# virtual platform hypercall
VP_HCALL_C_SRCS += arch/x86/guest/vmcall.c
VP_HCALL_C_SRCS += common/hypercall.c

# system initialization
SYS_INIT_C_SRCS += arch/x86/init.c

LIB_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(LIB_C_SRCS))
LIB_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(LIB_S_SRCS))
BOOT_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(BOOT_C_SRCS))
BOOT_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(BOOT_S_SRCS))
HW_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(HW_C_SRCS))
HW_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(HW_S_SRCS))
VM_CFG_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(realpath $(VM_CFG_C_SRCS)))
VP_BASE_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_BASE_C_SRCS))
VP_BASE_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(VP_BASE_S_SRCS))
VP_DM_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_DM_C_SRCS))
VP_TRUSTY_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_TRUSTY_C_SRCS))
VP_HCALL_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_HCALL_C_SRCS))
SYS_INIT_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(SYS_INIT_C_SRCS))

ifneq ($(CONFIG_RELEASE),y)
CFLAGS += -DHV_DEBUG -DPROFILING_ON -fno-omit-frame-pointer
endif
ifneq ($(FIRMWARE),uefi)
CFLAGS += -DCONFIG_LAST_LEVEL_EPT_AT_BOOT
endif

MODULES += $(LIB_MOD)
MODULES += $(BOOT_MOD)
MODULES += $(HW_MOD)
MODULES += $(VP_BASE_MOD)
MODULES += $(VP_DM_MOD)
MODULES += $(VP_TRUSTY_MOD)
MODULES += $(VP_HCALL_MOD)
ifeq ($(CONFIG_RELEASE),y)
MODULES += $(LIB_RELEASE)
LIB_BUILD = $(LIB_RELEASE)
LIB_MK = release/Makefile
else
MODULES += $(LIB_DEBUG)
LIB_BUILD = $(LIB_DEBUG)
LIB_MK = debug/Makefile
endif
MODULES += $(SYS_INIT_MOD)

DISTCLEAN_OBJS := $(shell find $(BASEDIR) -name '*.o')
VERSION := $(HV_OBJDIR)/include/version.h

PRE_BUILD_DIR := ../misc/hv_prebuild

.PHONY: all
all: pre_build $(HV_OBJDIR)/$(HV_FILE).32.out $(HV_OBJDIR)/$(HV_FILE).bin

install: $(HV_OBJDIR)/$(HV_FILE).32.out
	install -D $(HV_OBJDIR)/$(HV_FILE).32.out $(DESTDIR)$(libdir)/acrn/$(HV_FILE).$(BOARD).$(FIRMWARE).$(SCENARIO).32.out
	install -D $(HV_OBJDIR)/$(HV_FILE).bin $(DESTDIR)$(libdir)/acrn/$(HV_FILE).$(BOARD).$(FIRMWARE).$(SCENARIO).bin

install-debug: $(HV_OBJDIR)/$(HV_FILE).map $(HV_OBJDIR)/$(HV_FILE).out
	install -D $(HV_OBJDIR)/$(HV_FILE).out $(DESTDIR)$(libdir)/acrn/$(HV_FILE).$(BOARD).$(FIRMWARE).$(SCENARIO).out
	install -D $(HV_OBJDIR)/$(HV_FILE).map $(DESTDIR)$(libdir)/acrn/$(HV_FILE).$(BOARD).$(FIRMWARE).$(SCENARIO).map

.PHONY: pre_build
pre_build: $(HV_OBJDIR)/$(HV_CONFIG_H)
	@echo "Start pre-build static check ..."
	$(MAKE) -C $(PRE_BUILD_DIR) BOARD=$(BOARD) SCENARIO=$(SCENARIO) TARGET_DIR=$(TARGET_DIR)
	@$(HV_OBJDIR)/hv_prebuild_check.out

.PHONY: header
header: $(VERSION) $(HV_OBJDIR)/$(HV_CONFIG_H)

.PHONY: lib-mod boot-mod hw-mod vp-base-mod vp-dm-mod vp-trusty-mod vp-hcall-mod sys-init-mod
$(LIB_MOD): $(LIB_C_OBJS) $(LIB_S_OBJS)
	$(AR) $(ARFLAGS) $(LIB_MOD) $(LIB_C_OBJS) $(LIB_S_OBJS)

lib-mod: $(LIB_MOD)

$(BOOT_MOD): $(BOOT_S_OBJS) $(BOOT_C_OBJS)
	$(AR) $(ARFLAGS) $(BOOT_MOD) $(BOOT_S_OBJS) $(BOOT_C_OBJS)

boot-mod: $(BOOT_MOD)

$(HW_MOD): $(HW_S_OBJS) $(HW_C_OBJS) $(VM_CFG_C_OBJS)
	$(AR) $(ARFLAGS) $(HW_MOD) $(HW_S_OBJS) $(HW_C_OBJS) $(VM_CFG_C_OBJS)

hw-mod: $(HW_MOD)

$(VP_BASE_MOD): $(VP_BASE_S_OBJS) $(VP_BASE_C_OBJS)
	$(AR) $(ARFLAGS) $(VP_BASE_MOD) $(VP_BASE_S_OBJS) $(VP_BASE_C_OBJS)

vp-base-mod: $(VP_BASE_MOD)

$(VP_DM_MOD): $(VP_DM_C_OBJS)
	$(AR) $(ARFLAGS) $(VP_DM_MOD) $(VP_DM_C_OBJS)

vp-dm-mod: $(VP_DM_MOD)

$(VP_TRUSTY_MOD): $(VP_TRUSTY_C_OBJS)
	$(AR) $(ARFLAGS) $(VP_TRUSTY_MOD) $(VP_TRUSTY_C_OBJS)

vp-trusty-mod: $(VP_TRUSTY_MOD)

$(VP_HCALL_MOD): $(VP_HCALL_C_OBJS)
	$(AR) $(ARFLAGS) $(VP_HCALL_MOD) $(VP_HCALL_C_OBJS)

vp-hcall-mod: $(VP_HCALL_MOD)

$(SYS_INIT_MOD): $(SYS_INIT_C_OBJS)
	$(AR) $(ARFLAGS) $(SYS_INIT_MOD) $(SYS_INIT_C_OBJS)

sys-init-mod: $(SYS_INIT_MOD)

.PHONY: lib

$(LIB_BUILD): header
	$(MAKE) -f $(LIB_MK) MKFL_NAME=$(LIB_MK)

lib: $(LIB_BUILD)

$(HV_OBJDIR)/$(HV_FILE).32.out: $(HV_OBJDIR)/$(HV_FILE).out
	$(OBJCOPY) -S --section-alignment=0x1000 -O elf32-i386 $< $@

$(HV_OBJDIR)/$(HV_FILE).bin: $(HV_OBJDIR)/$(HV_FILE).out
	$(OBJCOPY) -O binary $< $(HV_OBJDIR)/$(HV_FILE).bin
	rm -f $(UPDATE_RESULT)

$(HV_OBJDIR)/$(HV_FILE).out: $(MODULES)
	${BASH} ${LD_IN_TOOL} $(ARCH_LDSCRIPT_IN) $(ARCH_LDSCRIPT) ${HV_OBJDIR}/.config
	$(CC) -Wl,-Map=$(HV_OBJDIR)/$(HV_FILE).map -o $@ $(LDFLAGS) $(ARCH_LDFLAGS) -T$(ARCH_LDSCRIPT) \
		-Wl,--start-group $^ -Wl,--end-group

.PHONY: clean
clean:
	rm -f $(VERSION)
	rm -rf $(HV_OBJDIR)

.PHONY: distclean
distclean:
	rm -f $(DISTCLEAN_OBJS)
	rm -f $(VERSION)
	rm -rf $(HV_OBJDIR)
	rm -f tags TAGS cscope.files cscope.in.out cscope.out cscope.po.out GTAGS GPATH GRTAGS GSYMS

PHONY: (VERSION)
$(VERSION): $(HV_OBJDIR)/$(HV_CONFIG_H)
	touch $(VERSION)
	@COMMIT=`git rev-parse --verify --short HEAD 2>/dev/null`;\
	DIRTY=`git diff-index --name-only HEAD`;\
	if [ -n "$$DIRTY" ];then PATCH="$$COMMIT-dirty";else PATCH="$$COMMIT";fi;\
	DAILY_TAG=`git tag --merged HEAD|grep "acrn"|tail -n 1`;\
	TIME=`date "+%F %T"`;\
	USER=`id -u -n`; \
	if [ x$(CONFIG_RELEASE) = "xy" ];then BUILD_TYPE="REL";else BUILD_TYPE="DBG";fi;\
	echo "/*" > $(VERSION); \
	sed 's/^/ * /' ../LICENSE >> $(VERSION); \
	echo " */" >> $(VERSION); \
	echo "" >> $(VERSION); \
	echo "#ifndef VERSION_H" >> $(VERSION); \
	echo "#define VERSION_H" >> $(VERSION); \
	echo "#define HV_FULL_VERSION "\"$(FULL_VERSION)\""" >> $(VERSION);\
	echo "#define HV_API_MAJOR_VERSION $(API_MAJOR_VERSION)U" >> $(VERSION);\
	echo "#define HV_API_MINOR_VERSION $(API_MINOR_VERSION)U" >> $(VERSION);\
	echo "#define HV_DAILY_TAG "\""$$DAILY_TAG"\""" >> $(VERSION);\
	echo "#define HV_BUILD_VERSION "\""$$PATCH"\""" >> $(VERSION);\
	echo "#define HV_BUILD_TYPE "\""$$BUILD_TYPE"\""" >> $(VERSION);\
	echo "#define HV_BUILD_TIME "\""$$TIME"\""" >> $(VERSION);\
	echo "#define HV_BUILD_USER "\""$$USER"\""" >> $(VERSION);\
	echo "#define HV_BUILD_SCENARIO "\"$(SCENARIO)\""" >> $(VERSION);\
	echo "#define HV_BUILD_BOARD "\"$(BOARD)\""" >> $(VERSION);\
	if [ "$(CONFIG_XML_ENABLED)" = "true" ]; then \
		echo "#define HV_CONFIG_TOOL \" with acrn-config\"" >> $(VERSION);\
	else	\
		echo "#define HV_CONFIG_TOOL \"\"" >> $(VERSION);\
	fi;\
	echo "#endif" >> $(VERSION)

-include $(C_OBJS:.o=.d)
-include $(S_OBJS:.o=.d)

$(HV_OBJDIR)/%.o: %.c $(VERSION) $(HV_OBJDIR)/$(HV_CONFIG_H)
	[ ! -e $@ ] && mkdir -p $(dir $@) && mkdir -p $(HV_MODDIR); \
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) -I. -c $(CFLAGS) $(ARCH_CFLAGS) $< -o $@ -MMD -MT $@

$(HV_OBJDIR)/%.o: %.S $(HV_OBJDIR)/$(HV_CONFIG_H)
	[ ! -e $@ ] && mkdir -p $(dir $@) && mkdir -p $(HV_MODDIR); \
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) -I. $(ASFLAGS) $(ARCH_ASFLAGS) -c $< -o $@ -MMD -MT $@

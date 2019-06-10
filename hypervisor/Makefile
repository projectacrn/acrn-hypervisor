#
# ACRN Hypervisor
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
SUB_MAKEFILES := $(wildcard */Makefile)

LIB_MOD = $(HV_MODDIR)/lib_mod.a
BOOT_MOD = $(HV_MODDIR)/boot_mod.a
HW_MOD = $(HV_MODDIR)/hw_mod.a
VP_BASE_MOD = $(HV_MODDIR)/vp_base_mod.a
LIB_DEBUG = $(HV_MODDIR)/libdebug.a
LIB_RELEASE = $(HV_MODDIR)/librelease.a

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

include $(BASEDIR)/../scripts/deps.mk

include scripts/kconfig/kconfig.mk

#initialize scenarios name
ifeq ($(CONFIG_SDC),y)
SCENARIO_NAME := sdc
else ifeq ($(CONFIG_LOGICAL_PARTITION),y)
SCENARIO_NAME := logical_partition
else ifeq ($(CONFIG_INDUSTRY),y)
SCENARIO_NAME := industry
else ifeq ($(CONFIG_HYBRID),y)
SCENARIO_NAME := hybrid
endif

LD_IN_TOOL = scripts/genld.sh
BASH = $(shell which bash)

ARFLAGS += crs

CFLAGS += -Wall -W
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -fshort-wchar -ffreestanding
CFLAGS += -fsigned-char
CFLAGS += -m64 -mno-mmx -mno-sse -mno-sse2 -mno-80387 -mno-fp-ret-in-387
CFLAGS += -mno-red-zone
CFLAGS += -nostdinc -nostdlib -fno-common
CFLAGS += -Werror
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

ASFLAGS += -m64 -nostdinc -nostdlib

LDFLAGS += -Wl,--gc-sections -nostartfiles -nostdlib
LDFLAGS += -Wl,-n,-z,max-page-size=0x1000

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
INCLUDE_PATH += arch/x86/configs/$(CONFIG_BOARD)
INCLUDE_PATH += scenarios/$(SCENARIO_NAME)

CC ?= gcc
AS ?= as
AR ?= ar
LD ?= ld
OBJCOPY ?= objcopy

export CC AS AR LD OBJCOPY
export CFLAGS ASFLAGS ARFLAGS LDFLAGS ARCH_CFLAGS ARCH_ASFLAGS ARCH_ARFLAGS ARCH_LDFLAGS
export HV_OBJDIR CONFIG_RELEASE INCLUDE_PATH
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
BOOT_C_SRCS += boot/reloc.c

# hardware management component
HW_S_SRCS += arch/x86/idt.S
HW_C_SRCS += arch/x86/ioapic.c
HW_C_SRCS += arch/x86/lapic.c
HW_C_SRCS += arch/x86/cpu.c
HW_C_SRCS += arch/x86/cpu_caps.c
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
HW_C_SRCS += arch/x86/cat.c
HW_C_SRCS += arch/x86/sgx.c
HW_C_SRCS += common/softirq.c
HW_C_SRCS += common/schedule.c
HW_C_SRCS += hw/pci.c
HW_C_SRCS += arch/x86/configs/vm_config.c
HW_C_SRCS += arch/x86/configs/$(CONFIG_BOARD)/board.c
HW_C_SRCS += scenarios/$(SCENARIO_NAME)/vm_configurations.c
ifneq (,$(wildcard scenarios/$(SCENARIO_NAME)/pt_dev.c))
HW_C_SRCS += scenarios/$(SCENARIO_NAME)/pt_dev.c
endif
HW_C_SRCS += boot/acpi_base.c
HW_C_SRCS += boot/dmar_info.c
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
VP_BASE_C_SRCS += arch/x86/guest/ucode.c
VP_BASE_C_SRCS += boot/guest/vboot_info.c
VP_BASE_C_SRCS += common/hv_main.c
VP_BASE_C_SRCS += common/vm_load.c
VP_BASE_C_SRCS += arch/x86/configs/vmptable.c
VP_BASE_C_SRCS += arch/x86/configs/$(CONFIG_BOARD)/ve820.c

# initilization component
C_SRCS += arch/x86/init.c

C_SRCS += arch/x86/seed/seed.c
C_SRCS += arch/x86/seed/seed_abl.c
C_SRCS += arch/x86/seed/seed_sbl.c


C_SRCS += arch/x86/guest/vlapic.c
C_SRCS += arch/x86/guest/vmcall.c
C_SRCS += arch/x86/guest/instr_emul.c
C_SRCS += arch/x86/guest/pm.c
C_SRCS += arch/x86/guest/assign.c
C_SRCS += arch/x86/guest/vmx_io.c
C_SRCS += arch/x86/guest/trusty.c
C_SRCS += arch/x86/guest/vm_reset.c
C_SRCS += common/hypercall.c
C_SRCS += common/trusty_hypercall.c
C_SRCS += common/ptdev.c
C_SRCS += dm/vpic.c
C_SRCS += dm/vrtc.c
C_SRCS += dm/vioapic.c
C_SRCS += dm/vuart.c
C_SRCS += dm/io_req.c
C_SRCS += dm/vpci/vdev.c
C_SRCS += dm/vpci/vpci.c
C_SRCS += dm/vpci/vhostbridge.c
C_SRCS += dm/vpci/pci_pt.c
C_SRCS += dm/vpci/vmsi.c
C_SRCS += dm/vpci/vmsix.c

LIB_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(LIB_C_SRCS))
LIB_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(LIB_S_SRCS))
BOOT_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(BOOT_C_SRCS))
BOOT_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(BOOT_S_SRCS))
HW_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(HW_C_SRCS))
HW_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(HW_S_SRCS))
VP_BASE_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_BASE_C_SRCS))
VP_BASE_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(VP_BASE_S_SRCS))
C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(C_SRCS))
ifneq ($(CONFIG_RELEASE),y)
CFLAGS += -DHV_DEBUG -DPROFILING_ON -fno-omit-frame-pointer
endif
S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(S_SRCS))
PRE_BUILD_SRCS += pre_build/static_checks.c
PRE_BUILD_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(PRE_BUILD_SRCS))

MOD_DEPS += lib-mod
MOD_DEPS += boot-mod
MOD_DEPS += hw-mod
MOD_DEPS += vp-base-mod
MOD_DEPS += lib
MODULES += $(LIB_MOD)
MODULES += $(BOOT_MOD)
MODULES += $(HW_MOD)
MODULES += $(VP_BASE_MOD)
ifeq ($(CONFIG_RELEASE),y)
MODULES += $(LIB_RELEASE)
else
MODULES += $(LIB_DEBUG)
endif
HV_OBJS += $(C_OBJS)
HV_OBJS += $(S_OBJS)

DISTCLEAN_OBJS := $(shell find $(BASEDIR) -name '*.o')
VERSION := $(HV_OBJDIR)/include/version.h

# Create platform_acpi_info.h
TEMPLATE_ACPI_INFO_HEADER := arch/x86/configs/platform_acpi_info.h
BOARDTEMPLATE_ACPI_INFO_HEADER := arch/x86/configs/$(CONFIG_BOARD)/platform_acpi_info.h
SOURCE_ACPI_INFO_HEADER := arch/x86/configs/$(CONFIG_BOARD)/$(CONFIG_BOARD)_acpi_info.h
TARGET_ACPI_INFO_HEADER := $(HV_OBJDIR)/include/platform_acpi_info.h

$(TARGET_ACPI_INFO_HEADER): $(HV_OBJDIR)/$(HV_CONFIG)
ifeq (,$(wildcard $(SOURCE_ACPI_INFO_HEADER)))
	@echo "******* No ACPI info found *******" && \
	 echo "Expected ACPI info header at $(SOURCE_ACPI_INFO_HEADER)" && \
	 echo "" && \
	 echo "The ACPI info header for this board is not available. Please use" && \
	 echo "an offline tool on the target board to generate a validated one." && \
	 echo "More details will be available soon."
ifdef CONFIG_ENFORCE_VALIDATED_ACPI_INFO
	@echo "" && \
	 echo "If you want to build the hypervisor with the template ACPI info," && \
	 echo "unset ENFORCE_VALIDATED_ACPI_INFO using 'make menuconfig'." && \
	 false
else ifeq (,$(wildcard $(BOARDTEMPLATE_ACPI_INFO_HEADER)))
	@echo "" && \
	 echo "Using the template at $(TEMPLATE_ACPI_INFO_HEADER) instead" && \
	 echo "**********************************"
	@cp $(TEMPLATE_ACPI_INFO_HEADER) $(TARGET_ACPI_INFO_HEADER)
else
	@echo "" && \
	 echo "Using the template at $(BOARDTEMPLATE_ACPI_INFO_HEADER) instead" && \
	 echo "**********************************"
	@cp $(BOARDTEMPLATE_ACPI_INFO_HEADER) $(TARGET_ACPI_INFO_HEADER)
endif
else
	@cp $(SOURCE_ACPI_INFO_HEADER) $(TARGET_ACPI_INFO_HEADER)
endif

.PHONY: all
all: pre_build $(MOD_DEPS) $(HV_OBJDIR)/$(HV_FILE).32.out $(HV_OBJDIR)/$(HV_FILE).bin

ifeq ($(FIRMWARE),sbl)
install: $(MOD_DEPS) $(HV_OBJDIR)/$(HV_FILE).32.out
ifeq ($(BOARD),apl-up2)
	install -D $(HV_OBJDIR)/$(HV_FILE).32.out $(DESTDIR)/usr/lib/acrn/$(HV_FILE).apl-up2.sbl
else
	install -D $(HV_OBJDIR)/$(HV_FILE).32.out $(DESTDIR)/usr/lib/acrn/$(HV_FILE).sbl
endif

install-debug: $(HV_OBJDIR)/$(HV_FILE).map $(HV_OBJDIR)/$(HV_FILE).out
ifeq ($(BOARD),apl-up2)
	install -D $(HV_OBJDIR)/$(HV_FILE).out $(DESTDIR)/usr/lib/acrn/$(HV_FILE).apl-up2.sbl.out
	install -D $(HV_OBJDIR)/$(HV_FILE).map $(DESTDIR)/usr/lib/acrn/$(HV_FILE).apl-up2.sbl.map
else
	install -D $(HV_OBJDIR)/$(HV_FILE).out $(DESTDIR)/usr/lib/acrn/$(HV_FILE).sbl.out
	install -D $(HV_OBJDIR)/$(HV_FILE).map $(DESTDIR)/usr/lib/acrn/$(HV_FILE).sbl.map
endif
endif

.PHONY: pre_build
pre_build: $(PRE_BUILD_OBJS)

.PHONY: header
header: $(VERSION) $(HV_OBJDIR)/$(HV_CONFIG_H) $(TARGET_ACPI_INFO_HEADER)

.PHONY: lib-mod boot-mod hw-mod vp-base-mod
lib-mod: $(LIB_C_OBJS) $(LIB_S_OBJS)
	$(AR) $(ARFLAGS) $(LIB_MOD) $(LIB_C_OBJS) $(LIB_S_OBJS)
boot-mod: $(BOOT_S_OBJS) $(BOOT_C_OBJS)
	$(AR) $(ARFLAGS) $(BOOT_MOD) $(BOOT_S_OBJS) $(BOOT_C_OBJS)
hw-mod: $(HW_S_OBJS) $(HW_C_OBJS)
	$(AR) $(ARFLAGS) $(HW_MOD) $(HW_S_OBJS) $(HW_C_OBJS)
vp-base-mod: $(VP_BASE_S_OBJS) $(VP_BASE_C_OBJS)
	$(AR) $(ARFLAGS) $(VP_BASE_MOD) $(VP_BASE_S_OBJS) $(VP_BASE_C_OBJS)
.PHONY: lib
lib: $(SUB_MAKEFILES)

.PHONY: $(SUB_MAKEFILES)
$(SUB_MAKEFILES): header
	for Makefile in $(SUB_MAKEFILES); do \
		$(MAKE) -f $$Makefile MKFL_NAME=$$Makefile; \
	done

$(HV_OBJDIR)/$(HV_FILE).32.out: $(HV_OBJDIR)/$(HV_FILE).out
	$(OBJCOPY) -S --section-alignment=0x1000 -O elf32-i386 $< $@

$(HV_OBJDIR)/$(HV_FILE).bin: $(HV_OBJDIR)/$(HV_FILE).out
	$(OBJCOPY) -O binary $< $(HV_OBJDIR)/$(HV_FILE).bin

$(HV_OBJDIR)/$(HV_FILE).out: $(MODULES) $(HV_OBJS)
	${BASH} ${LD_IN_TOOL} $(ARCH_LDSCRIPT_IN) $(ARCH_LDSCRIPT) ${HV_OBJDIR}/.config
	$(CC) -Wl,-Map=$(HV_OBJDIR)/$(HV_FILE).map -o $@ $(LDFLAGS) $(ARCH_LDFLAGS) -T$(ARCH_LDSCRIPT) \
		-Wl,--start-group $^ -Wl,--end-group

$(LIB_FLAGS): lib

.PHONY: clean
clean:
	rm -f $(C_OBJS)
	rm -f $(S_OBJS)
	rm -f $(VERSION)
	rm -rf $(HV_OBJDIR)

.PHONY: distclean
distclean:
	rm -f $(DISTCLEAN_OBJS)
	rm -f $(C_OBJS)
	rm -f $(S_OBJS)
	rm -f $(VERSION)
	rm -rf $(HV_OBJDIR)
	rm -f tags TAGS cscope.files cscope.in.out cscope.out cscope.po.out GTAGS GPATH GRTAGS GSYMS

PHONY: (VERSION)
$(VERSION):
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
	echo "#endif" >> $(VERSION)

-include $(C_OBJS:.o=.d)
-include $(S_OBJS:.o=.d)

$(HV_OBJDIR)/%.o: %.c $(VERSION) $(HV_OBJDIR)/$(HV_CONFIG_H) $(TARGET_ACPI_INFO_HEADER)
	[ ! -e $@ ] && mkdir -p $(dir $@) && mkdir -p $(HV_MODDIR); \
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) -I. -c $(CFLAGS) $(ARCH_CFLAGS) $< -o $@ -MMD -MT $@

$(HV_OBJDIR)/%.o: %.S $(HV_OBJDIR)/$(HV_CONFIG_H)
	[ ! -e $@ ] && mkdir -p $(dir $@) && mkdir -p $(HV_MODDIR); \
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) -I. $(ASFLAGS) $(ARCH_ASFLAGS) -c $< -o $@ -MMD -MT $@

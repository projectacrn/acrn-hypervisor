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
HV_FILE := acrn
SUB_MAKEFILES := $(wildcard */Makefile)

LIB_DEBUG = $(HV_OBJDIR)/debug/libdebug.a
LIB_RELEASE = $(HV_OBJDIR)/release/librelease.a

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
INCLUDE_PATH += include/debug
INCLUDE_PATH += include/public
INCLUDE_PATH += include/dm
INCLUDE_PATH += bsp/include
INCLUDE_PATH += boot/include
INCLUDE_PATH += $(HV_OBJDIR)/include
INCLUDE_PATH += arch/x86/configs/$(CONFIG_BOARD)

CC ?= gcc
AS ?= as
AR ?= ar
LD ?= ld
OBJCOPY ?= objcopy

export CC AS AR LD OBJCOPY
export CFLAGS ASFLAGS ARFLAGS LDFLAGS ARCH_CFLAGS ARCH_ASFLAGS ARCH_ARFLAGS ARCH_LDFLAGS
export HV_OBJDIR CONFIG_RELEASE INCLUDE_PATH
export LIB_DEBUG LIB_RELEASE

# boot component
S_SRCS += arch/x86/boot/cpu_primary.S
S_SRCS += arch/x86/boot/cpu_save_boot_ctx.S
S_SRCS += arch/x86/boot/trampoline.S
C_SRCS += boot/reloc.c

# initilization component
C_SRCS += arch/x86/init.c

# configuration component
C_SRCS += arch/x86/configs/vm_config.c
ifeq ($(CONFIG_PARTITION_MODE),y)
C_SRCS += arch/x86/configs/$(CONFIG_BOARD)/ve820.c
C_SRCS += arch/x86/configs/$(CONFIG_BOARD)/pt_dev.c
endif

C_SRCS += arch/x86/configs/$(CONFIG_BOARD)/board.c

C_SRCS += boot/acpi.c
C_SRCS += boot/dmar_parse.c
S_SRCS += arch/x86/idt.S
C_SRCS += arch/x86/ioapic.c
C_SRCS += arch/x86/lapic.c
C_SRCS += arch/x86/cpu.c
C_SRCS += arch/x86/cpu_caps.c
C_SRCS += arch/x86/security.c
C_SRCS += arch/x86/mmu.c
C_SRCS += arch/x86/e820.c
C_SRCS += arch/x86/pagetable.c
C_SRCS += arch/x86/page.c
C_SRCS += arch/x86/notify.c
C_SRCS += arch/x86/vtd.c
C_SRCS += arch/x86/gdt.c
C_SRCS += arch/x86/irq.c
C_SRCS += arch/x86/timer.c
C_SRCS += arch/x86/vmx.c
C_SRCS += arch/x86/cpu_state_tbl.c
C_SRCS += arch/x86/pm.c
S_SRCS += arch/x86/wakeup.S
C_SRCS += arch/x86/static_checks.c
C_SRCS += arch/x86/trampoline.c
S_SRCS += arch/x86/sched.S
C_SRCS += arch/x86/guest/vcpuid.c
C_SRCS += arch/x86/guest/vcpu.c
C_SRCS += arch/x86/guest/vm.c
C_SRCS += arch/x86/guest/vlapic.c
C_SRCS += arch/x86/guest/vmtrr.c
C_SRCS += arch/x86/guest/guest_memory.c
C_SRCS += arch/x86/guest/vmcall.c
C_SRCS += arch/x86/guest/vmsr.c
C_SRCS += arch/x86/guest/instr_emul.c
C_SRCS += arch/x86/guest/ucode.c
C_SRCS += arch/x86/guest/pm.c
C_SRCS += arch/x86/guest/assign.c
C_SRCS += arch/x86/guest/ept.c
C_SRCS += arch/x86/guest/io_emul.c
C_SRCS += arch/x86/guest/virq.c
C_SRCS += arch/x86/guest/virtual_cr.c
C_SRCS += arch/x86/guest/vmcs.c
C_SRCS += arch/x86/guest/vmexit.c
S_SRCS += arch/x86/guest/vmx_asm.S
C_SRCS += arch/x86/guest/trusty.c
C_SRCS += arch/x86/cat.c
C_SRCS += lib/misc.c
C_SRCS += lib/string.c
C_SRCS += lib/memory.c
C_SRCS += lib/crypto/crypto_api.c
C_SRCS += lib/crypto/mbedtls/hkdf.c
C_SRCS += lib/crypto/mbedtls/sha256.c
C_SRCS += lib/crypto/mbedtls/md.c
C_SRCS += lib/crypto/mbedtls/md_wrap.c
C_SRCS += lib/sprintf.c
C_SRCS += common/softirq.c
C_SRCS += common/hv_main.c
C_SRCS += common/hypercall.c
C_SRCS += common/trusty_hypercall.c
C_SRCS += common/schedule.c
C_SRCS += common/vm_load.c
C_SRCS += common/io_req.c
C_SRCS += common/ptdev.c
C_SRCS += common/static_checks.c

ifdef STACK_PROTECTOR
C_SRCS += common/stack_protector.c
endif

C_SRCS += hw/pci.c

C_SRCS += dm/vpic.c
C_SRCS += dm/vioapic.c
C_SRCS += dm/vpci/vdev.c
C_SRCS += dm/vpci/vpci.c
ifeq ($(CONFIG_PARTITION_MODE),y)
C_SRCS += dm/vmptable.c
C_SRCS += dm/vpci/partition_mode.c
C_SRCS += dm/vpci/hostbridge.c
C_SRCS += dm/vpci/pci_pt.c
C_SRCS += dm/vrtc.c
else
C_SRCS += dm/vpci/sharing_mode.c
C_SRCS += dm/vpci/msi.c
C_SRCS += dm/vpci/msix.c
endif

C_SRCS += bsp/firmware_wrapper.c
C_SRCS += bsp/firmware_sbl.c
C_SRCS += bsp/firmware_uefi.c
C_SRCS += bsp/cmdline.c
C_SRCS += bsp/const_dmar.c

ifeq ($(CONFIG_PLATFORM_UEFI),y)
C_SRCS += boot/uefi/uefi_boot.c
else
ifeq ($(CONFIG_PLATFORM_SBL),y)
C_SRCS += boot/sbl/multiboot.c
C_SRCS += boot/sbl/sbl_seed_parse.c
C_SRCS += boot/sbl/abl_seed_parse.c
endif
endif

# retpoline support
ifeq (true, $(shell [ $(GCC_MAJOR) -eq 7 ] && [ $(GCC_MINOR) -ge 3 ] && echo true))
CFLAGS += -mindirect-branch=thunk-extern -mindirect-branch-register
CFLAGS += -DCONFIG_RETPOLINE
S_SRCS += arch/x86/retpoline-thunk.S
else
ifeq (true, $(shell [ $(GCC_MAJOR) -ge 8 ] && echo true))
CFLAGS += -mindirect-branch=thunk-extern -mindirect-branch-register
CFLAGS += -DCONFIG_RETPOLINE
S_SRCS += arch/x86/retpoline-thunk.S
endif
endif

C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(C_SRCS))
ifneq ($(CONFIG_RELEASE),y)
CFLAGS += -DHV_DEBUG -DPROFILING_ON -fno-omit-frame-pointer
endif
S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(S_SRCS))

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
all: lib $(HV_OBJDIR)/$(HV_FILE).32.out $(HV_OBJDIR)/$(HV_FILE).bin

ifeq ($(CONFIG_PLATFORM_SBL),y)
install: lib $(HV_OBJDIR)/$(HV_FILE).32.out
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

.PHONY: header
header: $(VERSION) $(HV_OBJDIR)/$(HV_CONFIG_H) $(TARGET_ACPI_INFO_HEADER)

.PHONY: lib
lib: $(SUB_MAKEFILES)

.PHONY: $(SUB_MAKEFILES)
$(SUB_MAKEFILES): header
	for Makefile in $(SUB_MAKEFILES); do \
		$(MAKE) -f $$Makefile MKFL_NAME=$$Makefile; \
	done

ifneq ($(CONFIG_RELEASE),y)
LIB_FLAGS += $(LIB_DEBUG)
else
LIB_FLAGS += $(LIB_RELEASE)
endif

$(HV_OBJDIR)/$(HV_FILE).32.out: $(HV_OBJDIR)/$(HV_FILE).out
	$(OBJCOPY) -S --section-alignment=0x1000 -O elf32-i386 $< $@

$(HV_OBJDIR)/$(HV_FILE).bin: $(HV_OBJDIR)/$(HV_FILE).out
	$(OBJCOPY) -O binary $< $(HV_OBJDIR)/$(HV_FILE).bin

$(HV_OBJDIR)/$(HV_FILE).out: $(C_OBJS) $(S_OBJS) $(LIB_FLAGS)
	${BASH} ${LD_IN_TOOL} $(ARCH_LDSCRIPT_IN) $(ARCH_LDSCRIPT) ${HV_OBJDIR}/.config
	$(CC) -Wl,-Map=$(HV_OBJDIR)/$(HV_FILE).map -o $@ $(LDFLAGS) $(ARCH_LDFLAGS) -T$(ARCH_LDSCRIPT) $^

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
	[ ! -e $@ ] && mkdir -p $(dir $@); \
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) -I. -c $(CFLAGS) $(ARCH_CFLAGS) $< -o $@ -MMD -MT $@

$(HV_OBJDIR)/%.o: %.S $(HV_OBJDIR)/$(HV_CONFIG_H)
	[ ! -e $@ ] && mkdir -p $(dir $@); \
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) -I. $(ASFLAGS) $(ARCH_ASFLAGS) -c $< -o $@ -MMD -MT $@

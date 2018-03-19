#
# ACRN Hypervisor
#

MAJOR_VERSION=0
MINOR_VERSION=1
RC_VERSION=2

API_MAJOR_VERSION=1
API_MINOR_VERSION=0

RELEASE ?= 0

GCC_MAJOR=$(shell echo __GNUC__ | $(CC) -E -x c - | tail -n 1)
GCC_MINOR=$(shell echo __GNUC_MINOR__ | $(CC) -E -x c - | tail -n 1)

#enable stack overflow check
STACK_PROTECTOR := 1

BASEDIR := $(shell pwd)
PLATFORM ?= sbl
HV_OBJDIR ?= $(CURDIR)/build
HV_FILE := acrn

CFLAGS += -Wall -W
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -fshort-wchar -ffreestanding
CFLAGS += -m64
CFLAGS += -mno-red-zone
CFLAGS += -static -nostdinc -nostdlib -fno-common
CFLAGS += -O2 -D_FORTIFY_SOURCE=2
CFLAGS += -Wformat -Wformat-security

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

LDFLAGS += -Wl,--gc-sections -static -nostartfiles -nostdlib
LDFLAGS += -Wl,-n,-z,max-page-size=0x1000
LDFLAGS += -Wl,-z,noexecstack

ARCH_CFLAGS += -gdwarf-2 -O0
ARCH_ASFLAGS += -gdwarf-2 -DASSEMBLER=1
ARCH_ARFLAGS +=
ARCH_LDFLAGS +=

ARCH_LDSCRIPT = $(HV_OBJDIR)/link_ram.ld
ARCH_LDSCRIPT_IN = bsp/ld/link_ram.ld.in

INCLUDE_PATH += include
INCLUDE_PATH += include/lib
INCLUDE_PATH += include/common
INCLUDE_PATH += include/arch/x86
INCLUDE_PATH += include/arch/x86/guest
INCLUDE_PATH += include/debug
INCLUDE_PATH += include/public
INCLUDE_PATH += include/common
INCLUDE_PATH += bsp/include
INCLUDE_PATH += bsp/$(PLATFORM)/include/bsp
INCLUDE_PATH += boot/include

CC = gcc
AS = as
AR = ar
LD = gcc
POSTLD = objcopy

D_SRCS += debug/dump.c
D_SRCS += debug/logmsg.c
D_SRCS += debug/shell_internal.c
D_SRCS += debug/shell_public.c
D_SRCS += debug/vuart.c
D_SRCS += debug/serial.c
D_SRCS += debug/uart16550.c
D_SRCS += debug/console.c
D_SRCS += debug/sbuf.c
D_SRCS += debug/printf.c
C_SRCS += boot/acpi.c
C_SRCS += boot/dmar_parse.c
C_SRCS += arch/x86/ioapic.c
C_SRCS += arch/x86/intr_lapic.c
S_SRCS += arch/x86/cpu_secondary.S
C_SRCS += arch/x86/cpu.c
C_SRCS += arch/x86/softirq.c
C_SRCS += arch/x86/cpuid.c
C_SRCS += arch/x86/mmu.c
C_SRCS += arch/x86/notify.c
C_SRCS += arch/x86/intr_main.c
C_SRCS += arch/x86/vtd.c
C_SRCS += arch/x86/gdt.c
S_SRCS += arch/x86/cpu_primary.S
S_SRCS += arch/x86/idt.S
C_SRCS += arch/x86/irq.c
C_SRCS += arch/x86/timer.c
C_SRCS += arch/x86/ept.c
S_SRCS += arch/x86/vmx_asm.S
C_SRCS += arch/x86/io.c
C_SRCS += arch/x86/interrupt.c
C_SRCS += arch/x86/vmexit.c
C_SRCS += arch/x86/vmx.c
C_SRCS += arch/x86/assign.c
C_SRCS += arch/x86/trusty.c
C_SRCS += arch/x86/guest/vcpu.c
C_SRCS += arch/x86/guest/vm.c
C_SRCS += arch/x86/guest/instr_emul_wrapper.c
C_SRCS += arch/x86/guest/vlapic.c
C_SRCS += arch/x86/guest/guest.c
C_SRCS += arch/x86/guest/vmcall.c
C_SRCS += arch/x86/guest/vpic.c
C_SRCS += arch/x86/guest/vmsr.c
C_SRCS += arch/x86/guest/vioapic.c
C_SRCS += arch/x86/guest/instr_emul.c
C_SRCS += lib/spinlock.c
C_SRCS += lib/udelay.c
C_SRCS += lib/strnlen.c
C_SRCS += lib/memchr.c
C_SRCS += lib/stdlib.c
C_SRCS += lib/memcpy.c
C_SRCS += lib/strtol.c
C_SRCS += lib/mdelay.c
C_SRCS += lib/div.c
C_SRCS += lib/strchr.c
C_SRCS += lib/strcpy.c
C_SRCS += lib/memset.c
C_SRCS += lib/mem_mgt.c
C_SRCS += lib/strncpy.c
C_SRCS += lib/crypto/tinycrypt/hmac.c
C_SRCS += lib/crypto/tinycrypt/sha256.c
C_SRCS += lib/crypto/hkdf.c
C_SRCS += common/hv_main.c
C_SRCS += common/hypercall.c
C_SRCS += common/schedule.c
C_SRCS += common/vm_load.c

ifdef STACK_PROTECTOR
C_SRCS += common/stack_protector.c
endif

C_SRCS += bsp/$(PLATFORM)/vm_description.c
C_SRCS += bsp/$(PLATFORM)/$(PLATFORM).c

ifeq ($(PLATFORM),uefi)
C_SRCS += bsp/$(PLATFORM)/cmdline.c
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
ifeq ($(RELEASE),0)
C_OBJS += $(patsubst %.c,$(HV_OBJDIR)/%.o,$(D_SRCS))
CFLAGS += -DHV_DEBUG
endif
S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(S_SRCS))

DISTCLEAN_OBJS := $(shell find $(BASEDIR) -name '*.o')
VERSION := bsp/$(PLATFORM)/include/bsp/version.h

.PHONY: all
all: $(VERSION) $(HV_OBJDIR)/$(HV_FILE).32.out $(HV_OBJDIR)/$(HV_FILE).bin
	rm -f $(VERSION)

ifeq ($(PLATFORM), uefi)
all: efi
.PHONY: efi
efi: $(HV_OBJDIR)/$(HV_FILE).bin
	echo "building hypervisor as EFI executable..."
	make -C bsp/uefi/efi HV_OBJDIR=$(HV_OBJDIR) RELEASE=$(RELEASE)

install: efi
	make -C bsp/uefi/efi HV_OBJDIR=$(HV_OBJDIR) RELEASE=$(RELEASE) install
endif

$(HV_OBJDIR)/$(HV_FILE).32.out: $(HV_OBJDIR)/$(HV_FILE).out
	$(POSTLD) -S --section-alignment=0x1000 -O elf32-i386 $< $@

$(HV_OBJDIR)/$(HV_FILE).bin: $(HV_OBJDIR)/$(HV_FILE).out
	$(POSTLD) -O binary $< $(HV_OBJDIR)/$(HV_FILE).bin

$(HV_OBJDIR)/$(HV_FILE).out: $(C_OBJS) $(S_OBJS)
	$(CC) -E -x c $(patsubst %, -I%, $(INCLUDE_PATH)) $(ARCH_LDSCRIPT_IN) | grep -v '^#' > $(ARCH_LDSCRIPT)
	$(LD) -Wl,-Map=$(HV_OBJDIR)/$(HV_FILE).map -o $@ $(LDFLAGS) $(ARCH_LDFLAGS) -T$(ARCH_LDSCRIPT) $^

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
	TIME=`date "+%Y%m%d"`;\
	cat license_header > $(VERSION);\
	echo "#define HV_MAJOR_VERSION $(MAJOR_VERSION)" >> $(VERSION);\
	echo "#define HV_MINOR_VERSION $(MINOR_VERSION)" >> $(VERSION);\
	echo "#define HV_RC_VERSION $(RC_VERSION)" >> $(VERSION);\
	echo "#define HV_API_MAJOR_VERSION $(API_MAJOR_VERSION)" >> $(VERSION);\
	echo "#define HV_API_MINOR_VERSION $(API_MINOR_VERSION)" >> $(VERSION);\
	echo "#define HV_BUILD_VERSION "\""$$PATCH"\""" >> $(VERSION);\
	echo "#define HV_BUILD_TIME "\""$$TIME"\""" >> $(VERSION);\
	echo "#define HV_BUILD_USER "\""$(USER)"\""" >> $(VERSION)

$(HV_OBJDIR)/%.o: %.c
	[ ! -e $@ ] && mkdir -p $(dir $@); \
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) -I. -c $(CFLAGS) $(ARCH_CFLAGS) $< -o $@

$(HV_OBJDIR)/%.o: %.S
	[ ! -e $@ ] && mkdir -p $(dir $@); \
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) -I. $(ASFLAGS) $(ARCH_ASFLAGS) -c $< -o $@


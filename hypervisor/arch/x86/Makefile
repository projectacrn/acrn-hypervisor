#
# acrn-hypervisor/hypervisor/arch/x86/Makefile
#

ASL_COMPILER ?= $(shell which iasl)
IASL_MIN_VER ?= "20190703"

.PHONY: env_check
env_check:
ifndef ASL_COMPILER
$(error Please either install "iasl" or provide the path to "iasl" by using the ASL_COMPILER variable)
endif

LIB_MOD = $(HV_MODDIR)/lib_mod.a
BOOT_MOD = $(HV_MODDIR)/boot_mod.a
HW_MOD = $(HV_MODDIR)/hw_mod.a
VP_BASE_MOD = $(HV_MODDIR)/vp_base_mod.a
VP_DM_MOD = $(HV_MODDIR)/vp_dm_mod.a
VP_TRUSTY_MOD = $(HV_MODDIR)/vp_trusty_mod.a
VP_X86_TEE_MOD = $(HV_MODDIR)/vp_x86_tee_mod.a
VP_HCALL_MOD = $(HV_MODDIR)/vp_hcall_mod.a
LIB_DEBUG = $(HV_MODDIR)/libdebug.a
LIB_RELEASE = $(HV_MODDIR)/librelease.a
SYS_INIT_MOD = $(HV_MODDIR)/sys_init_mod.a

ARCH_CFLAGS += -m64 -mno-mmx -mno-sse -mno-sse2 -mno-80387 -mno-fp-ret-in-387
ARCH_CFLAGS += -mno-red-zone -mpopcnt

ARCH_LDSCRIPT_IN = bsp/ld/link_ram.ld.in

# In case using Ubuntu 19.10 as build environment, its gcc has -fcf-protection enabled
# by default. But this option is not compatible with -mindirect-branch:
#   https://bugs.launchpad.net/ubuntu/+source/gcc-9/+bug/1830961
#
# We disable the -fcf-protection in ACRN build.
ifeq (true, $(shell [ $(GCC_MAJOR) -gt 8 ] && echo true))
CFLAGS += -fcf-protection=none
endif

ASFLAGS += -m64 -nostdinc -nostdlib

# library componment
LIB_C_SRCS += arch/x86/lib/memory.c

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
BOOT_S_SRCS += arch/x86/boot/trampoline.S
BOOT_C_SRCS += arch/x86/rtcm.c

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
HW_C_SRCS += arch/x86/nmi.c
HW_C_SRCS += arch/x86/exception.c
HW_C_SRCS += arch/x86/irq.c
HW_C_SRCS += arch/x86/tsc.c
HW_C_SRCS += arch/x86/tsc_deadline_timer.c
HW_C_SRCS += arch/x86/hw_thermal.c
HW_C_SRCS += arch/x86/vmx.c
HW_C_SRCS += arch/x86/cpu_state_tbl.c
HW_C_SRCS += arch/x86/pm.c
HW_S_SRCS += arch/x86/wakeup.S
HW_C_SRCS += arch/x86/trampoline.c
HW_S_SRCS += arch/x86/sched.S
HW_C_SRCS += arch/x86/rdt.c
HW_C_SRCS += arch/x86/sgx.c
HW_C_SRCS += arch/x86/configs/vm_config.c
HW_C_SRCS += boot/acpi_base.c
# ACPI parsing component
# This part should be isolated from FuSa Cert
ifeq ($(CONFIG_ACPI_PARSE_ENABLED),y)
HW_C_SRCS += acpi_parser/dmar_parse.c
HW_C_SRCS += acpi_parser/acpi_ext.c
endif

# VM Configuration
VM_CFG_C_SRCS += $(BOARD_INFO_DIR)/board.c
VM_CFG_C_SRCS += $(SCENARIO_CFG_DIR)/vm_configurations.c
VM_CFG_C_SRCS += $(BOARD_CFG_DIR)/pt_intx.c
VM_CFG_C_SRCS += $(BOARD_CFG_DIR)/pci_dev.c

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
VP_BASE_C_SRCS += common/thermal.c
ifeq ($(CONFIG_HYPERV_ENABLED),y)
VP_BASE_C_SRCS += arch/x86/guest/hyperv.c
endif
ifeq ($(CONFIG_NVMX_ENABLED),y)
VP_BASE_C_SRCS += arch/x86/guest/nested.c
VP_BASE_C_SRCS += arch/x86/guest/vept.c
endif
VP_BASE_C_SRCS += arch/x86/configs/pci_dev.c
VP_BASE_C_SRCS += arch/x86/configs/vacpi.c
ifeq ($(CONFIG_SECURITY_VM_FIXUP),y)
VP_BASE_C_SRCS += quirks/security_vm_fixup.c
endif

# virtual platform device model
VP_DM_C_SRCS += dm/vpci/vhostbridge.c
VP_DM_C_SRCS += dm/vioapic.c
VP_DM_C_SRCS += dm/vpic.c
VP_DM_C_SRCS += arch/x86/guest/vlapic.c
VP_DM_C_SRCS += arch/x86/guest/pm.c
VP_DM_C_SRCS += arch/x86/guest/assign.c
VP_DM_C_SRCS += arch/x86/guest/vmx_io.c
VP_DM_C_SRCS += arch/x86/guest/instr_emul.c
VP_DM_C_SRCS += arch/x86/guest/lock_instr_emul.c
VP_DM_C_SRCS += arch/x86/guest/vm_reset.c
ifeq ($(CONFIG_VCAT_ENABLED),y)
VP_DM_C_SRCS += arch/x86/guest/vcat.c
endif

# virtual platform trusty
VP_TRUSTY_C_SRCS += arch/x86/guest/trusty.c
VP_TRUSTY_C_SRCS += common/trusty_hypercall.c
VP_TRUSTY_C_SRCS += arch/x86/seed/seed.c
VP_TRUSTY_C_SRCS += arch/x86/seed/seed_abl.c
VP_TRUSTY_C_SRCS += arch/x86/seed/seed_sbl.c

# x86 tee support
VP_X86_TEE_C_SRCS += arch/x86/guest/optee.c

# virtual platform hypercall
VP_HCALL_C_SRCS += arch/x86/guest/vmcall.c

# system initialization
SYS_INIT_C_SRCS += arch/x86/init.c

LIB_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(LIB_C_SRCS))
LIB_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(LIB_S_SRCS))
BOOT_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(BOOT_C_SRCS))
BOOT_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(BOOT_S_SRCS))
HW_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(HW_C_SRCS))
HW_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(HW_S_SRCS))
VM_CFG_C_OBJS := $(patsubst %.c,%.o,$(VM_CFG_C_SRCS))
VP_BASE_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_BASE_C_SRCS))
VP_BASE_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(VP_BASE_S_SRCS))
VP_DM_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_DM_C_SRCS))
VP_TRUSTY_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_TRUSTY_C_SRCS))
VP_X86_TEE_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_X86_TEE_C_SRCS))
VP_HCALL_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_HCALL_C_SRCS))
SYS_INIT_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(SYS_INIT_C_SRCS))

MODULES += $(LIB_MOD)
MODULES += $(BOOT_MOD)
MODULES += $(HW_MOD)
MODULES += $(VP_BASE_MOD)
MODULES += $(VP_DM_MOD)
MODULES += $(VP_TRUSTY_MOD)
MODULES += $(VP_X86_TEE_MOD)
MODULES += $(VP_HCALL_MOD)
MODULES += $(SYS_INIT_MOD)

HV_ACPI_TABLE_TIMESTAMP := $(HV_OBJDIR)/acpi.timestamp

PRE_BUILD_DIR := ../misc/hv_prebuild
PRE_BUILD_CHECKER := $(HV_OBJDIR)/hv_prebuild_check.out

.PHONY: lib-mod boot-mod hw-mod vp-base-mod vp-dm-mod vp-trusty-mod vp-x86tee-mod vp-hcall-mod sys-init-mod

$(HV_ACPI_TABLE_TIMESTAMP): $(HV_CONFIG_TIMESTAMP)
	@echo "generate the binary of ACPI tables for pre-launched VMs ..."
	python3 ../misc/config_tools/acpi_gen/bin_gen.py --board $(HV_BOARD_XML) --scenario $(HV_SCENARIO_XML) --asl $(HV_CONFIG_DIR) --out $(HV_OBJDIR) --iasl_path $(ASL_COMPILER) --iasl_min_ver $(IASL_MIN_VER)
	@touch $(HV_ACPI_TABLE_TIMESTAMP)

$(LIB_MOD): $(LIB_C_OBJS) $(LIB_S_OBJS)
	$(AR) $(ARFLAGS) $(LIB_MOD) $(LIB_C_OBJS) $(LIB_S_OBJS)

$(HV_OBJDIR)/$(HV_FILE).32.out: $(HV_OBJDIR)/$(HV_FILE).out
	$(OBJCOPY) -S --section-alignment=0x1000 -O elf32-i386 $< $@

$(PRE_BUILD_CHECKER): $(HV_CONFIG_H) $(HV_CONFIG_TIMESTAMP)
	@echo "Start pre-build static check ..."
	$(MAKE) -C $(PRE_BUILD_DIR) BOARD=$(BOARD) SCENARIO=$(SCENARIO) CHECKER_OUT=$(PRE_BUILD_CHECKER)
	@$(PRE_BUILD_CHECKER)

SERIAL_CONF = $(HV_OBJDIR)/serial.conf

$(SERIAL_CONF): $(HV_CONFIG_TIMESTAMP) $(HV_ALLOCATION_XML)
	@echo "generate the serial configuration file for service VM ..."
	python3 ../misc/config_tools/service_vm_config/serial_config.py --allocation $(HV_ALLOCATION_XML) --scenario $(HV_SCENARIO_XML) --out $(SERIAL_CONF)

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

$(VP_X86_TEE_MOD): $(VP_X86_TEE_C_OBJS)
	$(AR) $(ARFLAGS) $(VP_X86_TEE_MOD) $(VP_X86_TEE_C_OBJS)

vp-x86tee-mod: $(VP_X86_TEE_MOD)

$(VP_HCALL_MOD): $(VP_HCALL_C_OBJS)
	$(AR) $(ARFLAGS) $(VP_HCALL_MOD) $(VP_HCALL_C_OBJS)

vp-hcall-mod: $(VP_HCALL_MOD)

$(SYS_INIT_MOD): $(SYS_INIT_C_OBJS)
	$(AR) $(ARFLAGS) $(SYS_INIT_MOD) $(SYS_INIT_C_OBJS)

sys-init-mod: $(SYS_INIT_MOD)

arch-install: $(HV_OBJDIR)/$(HV_FILE).32.out
	install -D $(HV_OBJDIR)/$(HV_FILE).32.out $(DESTDIR)$(libdir)/acrn/$(HV_FILE).$(BOARD).$(SCENARIO).32.out
	@if [ -e "$(HV_OBJDIR)/acpi" ];then \
		install -D $(HV_OBJDIR)/acpi/*.bin -t $(DESTDIR)$(libdir)/acrn/acpi/; \
	fi

ARCH_PRE_BUILD_TARGETS := $(PRE_BUILD_CHECKER) $(SERIAL_CONF)
ARCH_ALL_TARGETS := env_check $(HV_ACPI_TABLE_TIMESTAMP) $(SERIAL_CONF) $(HV_OBJDIR)/$(HV_FILE).32.out
ARCH_INSTALL_TARGETS := arch-install

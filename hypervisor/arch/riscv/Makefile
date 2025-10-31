CROSS_COMPILE ?= riscv64-linux-gnu-

CC      = $(CROSS_COMPILE)gcc
AS      = $(CROSS_COMPILE)as
AR      = $(CROSS_COMPILE)ar
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

ARCH_LDSCRIPT_IN = arch/riscv/link_ram.ld.in

CFLAGS += -std=gnu99

ASFLAGS +=

LDFLAGS += -Wl,--no-warn-rwx-segments
LDFLAGS += -Wl,-z,norelro
LDFLAGS += -Wl,-z,noexecstack

ifeq (y, $(CONFIG_RELOC))
CFLAGS += -fpie
ASFLAGS += -fpie
CFLAGS += -fvisibility=hidden
LDFLAGS += -shared
LDFLAGS += -Wl,-z,notext
LDFLAGS += -Wl,-Bsymbolic
LDFLAGS += -Wl,-z,defs
else
CFLAGS += -fno-pie -static
ASFLAGS += -fno-pie -static
LDFLAGS += -static
endif

# g expands to i, m, a, f, d, zicsr and zifencei.
# h for hypervisor extension
RV_ISA += gh
RV_ISA += zbb
RV_ISA += zicbom
RV_ISA += zihintpause

ARCH_CFLAGS += -march=rv64$(subst $() $(),_,$(RV_ISA))
ARCH_CFLAGS += -mabi=lp64d
ARCH_CFLAGS += -mcmodel=medany

ARCH_ASFLAGS += -march=rv64$(subst $() $(),_,$(RV_ISA))

# HV host assembly sources
HOST_S_SRCS += arch/riscv/boot/cpu_entry.S
HOST_S_SRCS += arch/riscv/intr.S
HOST_S_SRCS += arch/riscv/sched.S

# HV host C sources
HOST_C_SRCS += arch/riscv/boot/reloc.c
HOST_C_SRCS += arch/riscv/lib/random.c
HOST_C_SRCS += arch/riscv/init.c
HOST_C_SRCS += arch/riscv/sbi.c
HOST_C_SRCS += arch/riscv/notify.c
HOST_C_SRCS += arch/riscv/timer.c
HOST_C_SRCS += arch/riscv/trap.c
HOST_C_SRCS += arch/riscv/cpu.c
HOST_C_SRCS += arch/riscv/mmu.c
HOST_C_SRCS += arch/riscv/irq.c
HOST_C_SRCS += arch/riscv/pgtable.c
HOST_C_SRCS += arch/riscv/security.c

# Virtual platform assembly sources
VP_S_SRCS +=

# Virtual platform C sources
VP_C_SRCS += arch/riscv/guest/guest_memory.c
VP_C_SRCS += arch/riscv/guest/vcpu.c
VP_C_SRCS += arch/riscv/guest/vm.c
VP_C_SRCS += arch/riscv/guest/vcpu_exit.c

VM_CFG_C_SRCS += $(SCENARIO_CFG_DIR)/vm_configurations.c
VM_CFG_C_SRCS += $(BOARD_CFG_DIR)/pci_dev.c
VM_CFG_C_SRCS += $(BOARD_CFG_DIR)/pt_intx.c
VM_CFG_C_SRCS += $(BOARD_INFO_DIR)/board.c

HOST_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(HOST_C_SRCS))
HOST_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(HOST_S_SRCS))
VP_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_C_SRCS))
VP_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(VP_S_SRCS))
VM_CFG_C_OBJS := $(patsubst %.c,%.o,$(VM_CFG_C_SRCS))

HOST_MOD := $(HV_MODDIR)/host_mod.a
VP_MOD := $(HV_MODDIR)/vp_mod.a

MODULES += $(HOST_MOD)
MODULES += $(VP_MOD)

$(HOST_MOD): $(HOST_C_OBJS) $(HOST_S_OBJS)
	$(AR) $(ARFLAGS) $(HOST_MOD) $(HOST_C_OBJS) $(HOST_S_OBJS)

$(VP_MOD): $(VP_C_OBJS) $(VP_S_OBJS) $(VM_CFG_C_OBJS)
	$(AR) $(ARFLAGS) $(VP_MOD) $(VP_C_OBJS) $(VP_S_OBJS) $(VM_CFG_C_OBJS)

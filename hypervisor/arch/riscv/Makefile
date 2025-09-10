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

# g expands to i, m, a, f, d, zicsr and zifencei.
# h for hypervisor extension
RV_ISA += gh
RV_ISA += zbb
RV_ISA += zicbom

ARCH_CFLAGS += -march=rv64$(subst $() $(),_,$(RV_ISA))
ARCH_CFLAGS += -mabi=lp64d
ARCH_CFLAGS += -mcmodel=medany

ARCH_ASFLAGS += -march=rv64$(subst $() $(),_,$(RV_ISA))

# HV host assembly sources
HOST_S_SRCS += arch/riscv/boot/cpu_entry.S

# HV host C sources
HOST_C_SRCS += arch/riscv/init.c
HOST_C_SRCS += arch/riscv/sbi.c
HOST_C_SRCS += arch/riscv/notify.c
HOST_C_SRCS += arch/riscv/timer.c
HOST_C_SRCS += arch/riscv/trap.c
HOST_C_SRCS += arch/riscv/cpu.c
HOST_C_SRCS += arch/riscv/logmsg.c

# Virtual platform assembly sources
VP_S_SRCS +=

# Virtual platform C sources
VP_C_SRCS += arch/riscv/guest/dummy_guest.c

HOST_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(HOST_C_SRCS))
HOST_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(HOST_S_SRCS))
VP_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_C_SRCS))
VP_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(VP_S_SRCS))

HOST_MOD := $(HV_MODDIR)/host_mod.a
VP_MOD := $(HV_MODDIR)/vp_mod.a

MODULES += $(HOST_MOD)
MODULES += $(VP_MOD)

$(HOST_MOD): $(HOST_C_OBJS) $(HOST_S_OBJS)
	$(AR) $(ARFLAGS) $(HOST_MOD) $(HOST_C_OBJS) $(HOST_S_OBJS)

$(VP_MOD): $(VP_C_OBJS) $(VP_S_OBJS)
	$(AR) $(ARFLAGS) $(VP_MOD) $(VP_C_OBJS) $(VP_S_OBJS)

#
# ACRN-DM
#
include ../VERSION
include ../paths.make

FULL_VERSION=$(MAJOR_VERSION).$(MINOR_VERSION)$(EXTRA_VERSION)
BASEDIR := $(shell pwd)
DM_OBJDIR ?= $(CURDIR)/build
DM_BUILD_VERSION ?=
DM_BUILD_TAG ?=

CC ?= gcc
RELEASE ?= 0

CFLAGS := -g -O0 -std=gnu11
CFLAGS += -D_GNU_SOURCE
CFLAGS += -DNO_OPENSSL
CFLAGS += -m64
CFLAGS += -Wall -ffunction-sections
CFLAGS += -Werror
CFLAGS += -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
CFLAGS += -Wformat -Wformat-security -fno-strict-aliasing
CFLAGS += -fno-delete-null-pointer-checks -fwrapv
CFLAGS += -fpie
CFLAGS += -Wno-stringop-truncation -Wno-address-of-packed-member

CFLAGS += -I$(BASEDIR)/include
CFLAGS += -I$(BASEDIR)/include/public
CFLAGS += -I$(DM_OBJDIR)/include
CFLAGS += -I$(TOOLS_OUT)

ifneq (, $(DM_ASL_COMPILER))
CFLAGS += -DASL_COMPILER=\"$(DM_ASL_COMPILER)\"
endif

GCC_MAJOR=$(shell echo __GNUC__ | $(CC) -E -x c - | tail -n 1)
GCC_MINOR=$(shell echo __GNUC_MINOR__ | $(CC) -E -x c - | tail -n 1)

#enable stack overflow check
STACK_PROTECTOR := 1

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
endif

ifeq ($(RELEASE),0)
CFLAGS += -DDM_DEBUG
else
LDFLAGS += -s
endif


LDFLAGS += -Wl,-z,noexecstack
LDFLAGS += -Wl,-z,relro,-z,now
LDFLAGS += -pie
LDFLAGS += -L$(TOOLS_OUT)

LIBS = -lrt
LIBS += -lpthread
LIBS += -lcrypto
LIBS += -lpciaccess
LIBS += -luuid
LIBS += -lusb-1.0
LIBS += -lacrn-mngr

# lib
SRCS += lib/dm_string.c

# hw
SRCS += hw/block_if.c
SRCS += hw/usb_core.c
SRCS += hw/uart_core.c
SRCS += hw/pci/virtio/virtio.c
SRCS += hw/pci/virtio/virtio_kernel.c
SRCS += hw/pci/virtio/vhost.c
SRCS += hw/platform/usb_mouse.c
SRCS += hw/platform/usb_pmapper.c
SRCS += hw/platform/atkbdc.c
SRCS += hw/platform/ps2mouse.c
SRCS += hw/platform/rtc.c
SRCS += hw/platform/pit.c
SRCS += hw/platform/hpet.c
SRCS += hw/platform/ps2kbd.c
SRCS += hw/platform/ioapic.c
SRCS += hw/platform/cmos_io.c
SRCS += hw/platform/ioc.c
SRCS += hw/platform/ioc_cbc.c
SRCS += hw/platform/pty_vuart.c
SRCS += hw/platform/acpi/acpi.c
SRCS += hw/platform/acpi/acpi_pm.c
SRCS += hw/platform/rpmb/rpmb_sim.c
SRCS += hw/platform/rpmb/rpmb_backend.c
SRCS += hw/platform/rpmb/att_keybox.c
SRCS += hw/platform/tpm/tpm_emulator.c
SRCS += hw/platform/tpm/tpm_crb.c
SRCS += hw/platform/tpm/tpm.c
SRCS += hw/platform/debugexit.c
SRCS += hw/pci/wdt_i6300esb.c
SRCS += hw/pci/lpc.c
SRCS += hw/pci/xhci.c
SRCS += hw/pci/core.c
SRCS += hw/pci/virtio/virtio_console.c
SRCS += hw/pci/virtio/virtio_block.c
SRCS += hw/pci/virtio/virtio_input.c
SRCS += hw/pci/virtio/virtio_i2c.c
SRCS += hw/pci/ahci.c
SRCS += hw/pci/hostbridge.c
SRCS += hw/pci/platform_gsi_info.c
SRCS += hw/pci/gsi_sharing.c
SRCS += hw/pci/passthrough.c
SRCS += hw/pci/virtio/virtio_audio.c
SRCS += hw/pci/virtio/virtio_net.c
SRCS += hw/pci/virtio/virtio_rnd.c
SRCS += hw/pci/virtio/virtio_ipu.c
SRCS += hw/pci/virtio/virtio_hyper_dmabuf.c
SRCS += hw/pci/virtio/virtio_mei.c
SRCS += hw/pci/virtio/virtio_coreu.c
SRCS += hw/pci/virtio/virtio_hdcp.c
SRCS += hw/pci/virtio/virtio_rpmb.c
SRCS += hw/pci/virtio/virtio_gpio.c
SRCS += hw/pci/irq.c
SRCS += hw/pci/uart.c
SRCS += hw/pci/gvt.c
SRCS += hw/pci/npk.c
SRCS += hw/pci/ivshmem.c
SRCS += hw/mmio/core.c

# core
#SRCS += core/bootrom.c
SRCS += core/monitor.c
SRCS += core/sw_load_common.c
SRCS += core/sw_load_bzimage.c
SRCS += core/sw_load_vsbl.c
SRCS += core/sw_load_ovmf.c
SRCS += core/sw_load_elf.c
SRCS += core/mevent.c
SRCS += core/gc.c
SRCS += core/pm.c
SRCS += core/pm_vuart.c
SRCS += core/console.c
SRCS += core/inout.c
SRCS += core/mem.c
SRCS += core/post.c
SRCS += core/vmmapi.c
SRCS += core/mptbl.c
SRCS += core/main.c
SRCS += core/hugetlb.c
SRCS += core/vrpmb.c
SRCS += core/timer.c

# arch
SRCS += arch/x86/pm.c
SRCS += arch/x86/power_button.c

# log
SRCS += log/log.c
SRCS += log/kmsg_logger.c
SRCS += log/disk_logger.c

OBJS := $(patsubst %.c,$(DM_OBJDIR)/%.o,$(SRCS))

VERSION_H := $(DM_OBJDIR)/include/version.h

HEADERS := $(shell find $(BASEDIR) -name '*.h')
HEADERS += $(VERSION_H)

DISTCLEAN_OBJS := $(shell find $(BASEDIR) -name '*.o')

PROGRAM := acrn-dm

SAMPLES_NUC := $(wildcard samples/nuc/*)
SAMPLES_MRB := $(wildcard samples/apl-mrb/*)
SAMPLES_UP2 := $(wildcard samples/apl-up2/*)

BIOS_BIN := $(wildcard bios/*)

all: $(DM_OBJDIR)/$(PROGRAM)
	@echo -n ""

$(DM_OBJDIR)/$(PROGRAM): $(OBJS)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)

clean:
	rm -rf $(DM_OBJDIR)

distclean:
	rm -f $(DISTCLEAN_OBJS)
	rm -rf $(DM_OBJDIR)
	rm -f tags TAGS cscope.files cscope.in.out cscope.out cscope.po.out GTAGS GPATH GRTAGS GSYMS

$(VERSION_H):
	mkdir -p $(DM_OBJDIR)/include
	touch $(VERSION_H)
	if [ "$(DM_BUILD_VERSION)"x = x -o "$(DM_BUILD_TAG)"x = x ];then\
		COMMIT=`git rev-parse --verify --short HEAD 2>/dev/null`;\
		DIRTY=`git diff-index --name-only HEAD`;\
		if [ -n "$$DIRTY" ];then PATCH="$$COMMIT-dirty";else PATCH="$$COMMIT";fi;\
		DAILY_TAG=`git tag --merged HEAD|grep "acrn"|tail -n 1`;\
	else\
		PATCH=$(DM_BUILD_VERSION);\
		DAILY_TAG=$(DM_BUILD_TAG);\
	fi;\
	TIME=`date "+%Y-%m-%d %H:%M:%S"`;\
	USER=`id -u -n`; \
	echo "/*" > $(VERSION_H); \
	sed 's/^/ * /' ../LICENSE >> $(VERSION_H);\
	echo " */" >> $(VERSION_H);\
	echo "" >> $(VERSION_H);\
	echo "#define DM_MAJOR_VERSION $(MAJOR_VERSION)" >> $(VERSION_H);\
	echo "#define DM_MINOR_VERSION $(MINOR_VERSION)" >> $(VERSION_H);\
	echo "#define DM_EXTRA_VERSION "\"$(EXTRA_VERSION)\""" >> $(VERSION_H);\
	echo "#define DM_FULL_VERSION "\"$(FULL_VERSION)\""" >> $(VERSION_H);\
	echo "#define DM_DAILY_TAG "\""$$DAILY_TAG"\""" >> $(VERSION_H);\
	echo "#define DM_BUILD_VERSION "\""$$PATCH"\""" >> $(VERSION_H);\
	echo "#define DM_BUILD_TIME "\""$$TIME"\""" >> $(VERSION_H);\
	echo "#define DM_BUILD_USER "\""$$USER"\""" >> $(VERSION_H)

-include $(OBJS:.o=.d)

$(DM_OBJDIR)/%.o: %.c $(HEADERS)
	[ ! -e $@ ] && mkdir -p $(dir $@); \
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MT $@

install: $(DM_OBJDIR)/$(PROGRAM) install-samples-nuc install-samples-mrb install-bios install-samples-up2
	install -D --mode=0755 $(DM_OBJDIR)/$(PROGRAM) $(DESTDIR)$(bindir)/$(PROGRAM)

install-samples-up2: $(SAMPLES_UP2)
	install -D -t $(DESTDIR)$(datadir)/acrn/samples/apl-up2 $^

install-samples-nuc: $(SAMPLES_NUC)
	install -D -t $(DESTDIR)$(datadir)/acrn/samples/nuc $^

install-samples-mrb: $(SAMPLES_MRB)
	install -D -t $(DESTDIR)$(datadir)/acrn/samples/apl-mrb $^
	install -d $(DESTDIR)$(systemd_unitdir)/system/
	install -p -D -m 0644 ./samples/apl-mrb/acrn_guest.service $(DESTDIR)$(systemd_unitdir)/system

install-bios: $(BIOS_BIN)
	install -d $(DESTDIR)$(datadir)/acrn/bios
	install -D --mode=0664 -t $(DESTDIR)$(datadir)/acrn/bios $^

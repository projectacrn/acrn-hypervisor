#
# ACRN-DM
#
MAJOR_VERSION=0
MINOR_VERSION=1
RC_VERSION=2
BASEDIR := $(shell pwd)
DM_OBJDIR ?= $(CURDIR)/build

ifneq ($(TARGET_YOCTO), TRUE)
CC := gcc
endif

CFLAGS := -g -O0 -std=gnu11
CFLAGS += -D_GNU_SOURCE
CFLAGS += -DNO_OPENSSL
CFLAGS += -m64
CFLAGS += -Wall -ffunction-sections
CFLAGS += -Werror

CFLAGS += -I$(BASEDIR)/include
CFLAGS += -I$(BASEDIR)/include/public

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

LDFLAGS += -Wl,-z,noexecstack
LDFLAGS += -Wl,-z,relro,-z,now

LIBS = -lrt
LIBS += -lpthread
LIBS += -lcrypto
LIBS += -lpciaccess
LIBS += -lz
LIBS += -luuid

# hw
SRCS += hw/pci/virtio/virtio.c
SRCS += hw/pci/virtio/virtio_kernel.c
SRCS += hw/platform/usb_mouse.c
SRCS += hw/platform/usb_core.c
SRCS += hw/platform/atkbdc.c
SRCS += hw/platform/ps2mouse.c
SRCS += hw/platform/rtc.c
SRCS += hw/platform/ps2kbd.c
SRCS += hw/platform/pm.c
SRCS += hw/platform/uart_core.c
SRCS += hw/platform/block_if.c
SRCS += hw/platform/ioapic.c
SRCS += hw/platform/cmos_io.c
SRCS += hw/pci/wdt_i6300esb.c
SRCS += hw/pci/lpc.c
SRCS += hw/pci/xhci.c
SRCS += hw/pci/core.c
SRCS += hw/pci/virtio/virtio_console.c
SRCS += hw/pci/virtio/virtio_block.c
SRCS += hw/pci/ahci.c
SRCS += hw/pci/hostbridge.c
SRCS += hw/pci/passthrough.c
SRCS += hw/pci/virtio/virtio_net.c
SRCS += hw/pci/virtio/virtio_rnd.c
SRCS += hw/pci/virtio/virtio_hyper_dmabuf.c
SRCS += hw/pci/irq.c
SRCS += hw/pci/uart.c
SRCS += hw/acpi/acpi.c

# core
#SRCS += core/bootrom.c
SRCS += core/sw_load.c
SRCS += core/smbiostbl.c
SRCS += core/mevent.c
SRCS += core/gc.c
SRCS += core/console.c
SRCS += core/inout.c
SRCS += core/mem.c
SRCS += core/post.c
SRCS += core/consport.c
SRCS += core/vmmapi.c
SRCS += core/mptbl.c
SRCS += core/main.c


OBJS := $(patsubst %.c,$(DM_OBJDIR)/%.o,$(SRCS))

HEADERS := $(shell find $(BASEDIR) -name '*.h')
DISTCLEAN_OBJS := $(shell find $(BASEDIR) -name '*.o')

PROGRAM := acrn-dm

SAMPLES := $(wildcard samples/*)

all: include/version.h $(PROGRAM)
	@echo -n ""

$(PROGRAM): $(OBJS)
	$(CC) -o $(DM_OBJDIR)/$@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)

clean:
	rm -f $(OBJS)
	rm -f include/version.h
	rm -f $(OBJS)
	rm -rf $(DM_OBJDIR)
	if test -f $(PROGRAM); then rm $(PROGRAM); fi

distclean:
	rm -f $(DISTCLEAN_OBJS)
	rm -f include/version.h
	rm -f $(OBJS)
	rm -rf $(DM_OBJDIR)
	rm -f tags TAGS cscope.files cscope.in.out cscope.out cscope.po.out GTAGS GPATH GRTAGS GSYMS

include/version.h:
	touch include/version.h
	@COMMIT=`git rev-parse --verify --short HEAD 2>/dev/null`;\
	DIRTY=`git diff-index --name-only HEAD`;\
	if [ -n "$$DIRTY" ];then PATCH="$$COMMIT-dirty";else PATCH="$$COMMIT";fi;\
	TIME=`date "+%Y-%m-%d %H:%M:%S"`;\
	cat license_header > include/version.h;\
	echo "#define DM_MAJOR_VERSION $(MAJOR_VERSION)" >> include/version.h;\
	echo "#define DM_MINOR_VERSION $(MINOR_VERSION)" >> include/version.h;\
	echo "#define DM_RC_VERSION $(RC_VERSION)" >> include/version.h;\
	echo "#define DM_BUILD_VERSION "\""$$PATCH"\""" >> include/version.h;\
	echo "#define DM_BUILD_TIME "\""$$TIME"\""" >> include/version.h;\
	echo "#define DM_BUILD_USER "\""$(USER)"\""" >> include/version.h

$(DM_OBJDIR)/%.o: %.c $(HEADERS)
	[ ! -e $@ ] && mkdir -p $(dir $@); \
	$(CC) $(CFLAGS) -c $< -o $@

install: $(DM_OBJDIR)/$(PROGRAM) install-samples
	install -D $(DM_OBJDIR)/$(PROGRAM) $(DESTDIR)/usr/bin/$(PROGRAM)

install-samples: $(SAMPLES)
	install -d $(DESTDIR)/usr/share/acrn/demo
	install -t $(DESTDIR)/usr/share/acrn/demo $^

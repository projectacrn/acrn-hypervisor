include ../../../paths.make

T := $(CURDIR)
OUT_DIR ?= $(shell mkdir -p $(T)/build;cd $(T)/build;pwd)
CC ?= gcc

ifndef RELEASE
  override RELEASE := n
else
  # Backward-compatibility for RELEASE=(0|1)
  ifeq ($(RELEASE),1)
    override RELEASE := y
  else
    ifeq ($(RELEASE),0)
      override RELEASE := n
    endif
  endif
endif

MANAGER_CFLAGS := -g -O0 -std=gnu11
MANAGER_CFLAGS += -D_GNU_SOURCE
MANAGER_CFLAGS += -DNO_OPENSSL
MANAGER_CFLAGS += -m64
MANAGER_CFLAGS += -Wall -ffunction-sections
MANAGER_CFLAGS += -Werror
MANAGER_CFLAGS += -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
MANAGER_CFLAGS += -Wformat -Wformat-security -fno-strict-aliasing
MANAGER_CFLAGS += -fno-delete-null-pointer-checks -fwrapv
MANAGER_CFLAGS += -fpie -fpic
#FIXME: remove me. work-around for system() calls, which will be removed
MANAGER_CFLAGS += -Wno-format-truncation -Wno-unused-result
MANAGER_CFLAGS += -Wno-stringop-truncation
MANAGER_CFLAGS += $(CFLAGS)

MANAGER_CFLAGS += -I../../../devicemodel/include
MANAGER_CFLAGS += -I../../../devicemodel/include/public
MANAGER_CFLAGS += -I../../../hypervisor/include

MANAGER_HEADERS := ../../../devicemodel/include/dm.h
MANAGER_HEADERS += ../../../devicemodel/include/types.h
MANAGER_HEADERS += ../../../devicemodel/include/pm.h
MANAGER_HEADERS += ../../../devicemodel/include/dm_string.h
MANAGER_HEADERS += ../../../devicemodel/include/macros.h
MANAGER_HEADERS += ../../../devicemodel/include/public/vhm_ioctl_defs.h
MANAGER_HEADERS += ../../../devicemodel/include/public/acrn_common.h

GCC_MAJOR=$(shell echo __GNUC__ | $(CC) -E -x c - | tail -n 1)
GCC_MINOR=$(shell echo __GNUC_MINOR__ | $(CC) -E -x c - | tail -n 1)

#enable stack overflow check
STACK_PROTECTOR := 1

ifdef STACK_PROTECTOR
ifeq (true, $(shell [ $(GCC_MAJOR) -gt 4 ] && echo true))
MANAGER_CFLAGS += -fstack-protector-strong
else
ifeq (true, $(shell [ $(GCC_MAJOR) -eq 4 ] && [ $(GCC_MINOR) -ge 9 ] && echo true))
MANAGER_CFLAGS += -fstack-protector-strong
else
MANAGER_CFLAGS += -fstack-protector
endif
endif
endif

ifeq ($(RELEASE),n)
MANAGER_CFLAGS += -g -DMNGR_DEBUG
else
MANAGER_LDFLAGS += -s
endif

MANAGER_LDFLAGS := -Wl,-z,noexecstack
MANAGER_LDFLAGS += -Wl,-z,relro,-z,now
MANAGER_LDFLAGS += -pie
MANAGER_LDFLAGS += -L$(OUT_DIR)
MANAGER_LDFLAGS +=  -lpthread
MANAGER_LDFLAGS += -lacrn-mngr
MANAGER_LDFLAGS += $(LDFLAGS)

.PHONY: all
all: $(OUT_DIR)/libacrn-mngr.a $(OUT_DIR)/acrn_mngr.h $(OUT_DIR)/acrnctl $(OUT_DIR)/acrnd

$(OUT_DIR)/libacrn-mngr.a: acrn_mngr.c acrn_mngr.h $(MANAGER_HEADERS)
	$(CC) $(MANAGER_CFLAGS) -c acrn_mngr.c -o $(OUT_DIR)/acrn_mngr.o
	ar -cr $@ $(OUT_DIR)/acrn_mngr.o

ifneq ($(OUT_DIR),.)
$(OUT_DIR)/acrn_mngr.h: ./acrn_mngr.h $(MANAGER_HEADERS)
	cp ./acrn_mngr.h $(OUT_DIR)/
endif

$(OUT_DIR)/acrnctl: acrnctl.c acrn_mngr.h $(OUT_DIR)/libacrn-mngr.a
	$(CC) -o $(OUT_DIR)/acrnctl acrnctl.c acrn_vm_ops.c $(MANAGER_CFLAGS) $(MANAGER_LDFLAGS)

$(OUT_DIR)/acrnd: acrnd.c $(OUT_DIR)/libacrn-mngr.a
	$(CC) -o $(OUT_DIR)/acrnd acrnd.c acrn_vm_ops.c $(MANAGER_CFLAGS) $(MANAGER_LDFLAGS)
ifneq ($(OUT_DIR),.)
	cp ./acrnd.service $(OUT_DIR)/acrnd.service
endif

.PHONY: clean
clean:
	rm -f $(OUT_DIR)/acrnctl
	rm -f $(OUT_DIR)/acrn_mngr.o
	rm -f $(OUT_DIR)/libacrn-mngr.a
	rm -f $(OUT_DIR)/acrnd
ifneq ($(OUT_DIR),.)
	rm -f $(OUT_DIR)/acrn_mngr.h
	rm -f $(OUT_DIR)/acrnd.service
	rm -rf $(OUT_DIR)
endif

.PHONY: install
install:
	install -d $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(systemd_unitdir)/system
	install -d $(DESTDIR)$(libdir)
	install -d $(DESTDIR)$(includedir)/acrn
	install -t $(DESTDIR)$(bindir) $(OUT_DIR)/acrnctl
	install -t $(DESTDIR)$(bindir) $(OUT_DIR)/acrnd
	install -m 0644 -t $(DESTDIR)$(libdir) $(OUT_DIR)/libacrn-mngr.a
	install -m 0644 -t $(DESTDIR)$(includedir)/acrn $(OUT_DIR)/acrn_mngr.h
	install -m 0644 -t $(DESTDIR)$(includedir)/acrn $(MANAGER_HEADERS)
	install -p -D -m 0644 $(OUT_DIR)/acrnd.service $(DESTDIR)$(systemd_unitdir)/system

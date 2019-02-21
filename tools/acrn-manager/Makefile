T := $(CURDIR)
OUT_DIR ?= $(shell mkdir -p $(T)/build;cd $(T)/build;pwd)
CC ?= gcc
RELEASE ?= 0

CFLAGS := -g -O0 -std=gnu11
CFLAGS += -D_GNU_SOURCE
CFLAGS += -DNO_OPENSSL
CFLAGS += -m64
CFLAGS += -Wall -ffunction-sections
CFLAGS += -Werror
CFLAGS += -O2 -D_FORTIFY_SOURCE=2
CFLAGS += -Wformat -Wformat-security -fno-strict-aliasing
CFLAGS += -fpie -fpic
#FIXME: remove me. work-around for system() calls, which will be removed
CFLAGS += -Wno-format-truncation -Wno-unused-result

CFLAGS += -I../../devicemodel/include
CFLAGS += -I../../devicemodel/include/public
CFLAGS += -I../../hypervisor/include

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
CFLAGS += -g -DMNGR_DEBUG
endif

LDFLAGS := -Wl,-z,noexecstack
LDFLAGS += -Wl,-z,relro,-z,now
LDFLAGS += -pie
LDFLAGS += -L$(OUT_DIR)
LDFLAGS +=  -lpthread
LDFLAGS += -lacrn-mngr

.PHONY: all
ifeq ($(RELEASE),0)
all: $(OUT_DIR)/libacrn-mngr.a $(OUT_DIR)/acrn_mngr.h $(OUT_DIR)/acrnctl $(OUT_DIR)/acrnd
else
all: $(OUT_DIR)/libacrn-mngr.a $(OUT_DIR)/acrn_mngr.h $(OUT_DIR)/acrnd
endif

$(OUT_DIR)/libacrn-mngr.a: acrn_mngr.c acrn_mngr.h
	$(CC) $(CFLAGS) -c acrn_mngr.c -o $(OUT_DIR)/acrn_mngr.o
	ar -cr $@ $(OUT_DIR)/acrn_mngr.o

ifneq ($(OUT_DIR),.)
$(OUT_DIR)/acrn_mngr.h: ./acrn_mngr.h
	cp ./acrn_mngr.h $(OUT_DIR)/
endif

$(OUT_DIR)/acrnctl: acrnctl.c acrn_mngr.h $(OUT_DIR)/libacrn-mngr.a
	$(CC) -o $(OUT_DIR)/acrnctl acrnctl.c acrn_vm_ops.c $(CFLAGS) $(LDFLAGS)

$(OUT_DIR)/acrnd: acrnd.c $(OUT_DIR)/libacrn-mngr.a
	$(CC) -o $(OUT_DIR)/acrnd acrnd.c acrn_vm_ops.c $(CFLAGS) $(LDFLAGS)
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
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/lib/systemd/system
	install -d $(DESTDIR)/usr/lib64/
	install -d $(DESTDIR)/usr/include/acrn
ifeq ($(RELEASE),0)
	install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrnctl
endif
	install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrnd
	install -t $(DESTDIR)/usr/lib64/ $(OUT_DIR)/libacrn-mngr.a
	install -t $(DESTDIR)/usr/include/acrn $(OUT_DIR)/acrn_mngr.h
	install -p -D -m 0644 $(OUT_DIR)/acrnd.service $(DESTDIR)/usr/lib/systemd/system

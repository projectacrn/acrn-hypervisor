T := $(CURDIR)
OUT_DIR ?= $(shell mkdir -p $(T)/build;cd $(T)/build;pwd)
CC ?= gcc

CFLAGS := -g -O0 -std=gnu11
CFLAGS += -D_GNU_SOURCE
CFLAGS += -DNO_OPENSSL
CFLAGS += -m64
CFLAGS += -Wall -ffunction-sections
CFLAGS += -Werror
CFLAGS += -O2 -D_FORTIFY_SOURCE=2
CFLAGS += -Wformat -Wformat-security -fno-strict-aliasing
CFLAGS += -fpie -fpic

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

LDFLAGS := -Wl,-z,noexecstack
LDFLAGS += -Wl,-z,relro,-z,now
LDFLAGS += -pie

all:
	$(CC) -g acrnlog.c -o $(OUT_DIR)/acrnlog -lpthread $(CFLAGS) $(LDFLAGS)
	cp acrnlog.service $(OUT_DIR)/acrnlog.service

clean:
	rm -f $(OUT_DIR)/acrnlog
ifneq ($(OUT_DIR),.)
	rm -f $(OUT_DIR)/acrnlog.service
	rm -rf $(OUT_DIR)
endif

install: $(OUT_DIR)/acrnlog
	install -d $(DESTDIR)/usr/bin
	install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrnlog
	install -d $(DESTDIR)/usr/lib/systemd/system
	install -p -D -m 0644 $(OUT_DIR)/acrnlog.service $(DESTDIR)/usr/lib/systemd/system

T := $(CURDIR)
OUT_DIR ?= $(shell mkdir -p $(T)/build;cd $(T)/build;pwd)
CC ?= gcc

LOG_CFLAGS := -g -O0 -std=gnu11
LOG_CFLAGS += -D_GNU_SOURCE
LOG_CFLAGS += -DNO_OPENSSL
LOG_CFLAGS += -m64
LOG_CFLAGS += -Wall -ffunction-sections
LOG_CFLAGS += -Werror
LOG_CFLAGS += -O2 -D_FORTIFY_SOURCE=2
LOG_CFLAGS += -Wformat -Wformat-security -fno-strict-aliasing
LOG_CFLAGS += -fpie -fpic
LOG_CFLAGS += $(CFLAGS)

GCC_MAJOR=$(shell echo __GNUC__ | $(CC) -E -x c - | tail -n 1)
GCC_MINOR=$(shell echo __GNUC_MINOR__ | $(CC) -E -x c - | tail -n 1)

#enable stack overflow check
STACK_PROTECTOR := 1

ifdef STACK_PROTECTOR
ifeq (true, $(shell [ $(GCC_MAJOR) -gt 4 ] && echo true))
LOG_CFLAGS += -fstack-protector-strong
else
ifeq (true, $(shell [ $(GCC_MAJOR) -eq 4 ] && [ $(GCC_MINOR) -ge 9 ] && echo true))
LOG_CFLAGS += -fstack-protector-strong
else
LOG_CFLAGS += -fstack-protector
endif
endif
endif

LOG_LDFLAGS := -Wl,-z,noexecstack
LOG_LDFLAGS += -Wl,-z,relro,-z,now
LOG_LDFLAGS += -pie
LOG_LDFLAGS += $(LDFLAGS)

all:
	$(CC) -g acrnlog.c -o $(OUT_DIR)/acrnlog -lpthread $(LOG_CFLAGS) $(LOG_LDFLAGS)
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

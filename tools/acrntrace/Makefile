T := $(CURDIR)
OUT_DIR ?= $(shell mkdir -p $(T)/build;cd $(T)/build;pwd)
CC ?= gcc

TRACE_CFLAGS := -g -O0 -std=gnu11
TRACE_CFLAGS += -D_GNU_SOURCE
TRACE_CFLAGS += -DNO_OPENSSL
TRACE_CFLAGS += -m64
TRACE_CFLAGS += -Wall -ffunction-sections
TRACE_CFLAGS += -Werror
TRACE_CFLAGS += -O2 -D_FORTIFY_SOURCE=2
TRACE_CFLAGS += -Wformat -Wformat-security -fno-strict-aliasing
TRACE_CFLAGS += -fpie -fpic
TRACE_CFLAGS += $(CFLAGS)

GCC_MAJOR=$(shell echo __GNUC__ | $(CC) -E -x c - | tail -n 1)
GCC_MINOR=$(shell echo __GNUC_MINOR__ | $(CC) -E -x c - | tail -n 1)

#enable stack overflow check
STACK_PROTECTOR := 1

ifdef STACK_PROTECTOR
ifeq (true, $(shell [ $(GCC_MAJOR) -gt 4 ] && echo true))
TRACE_CFLAGS += -fstack-protector-strong
else
ifeq (true, $(shell [ $(GCC_MAJOR) -eq 4 ] && [ $(GCC_MINOR) -ge 9 ] && echo true))
TRACE_CFLAGS += -fstack-protector-strong
else
TRACE_CFLAGS += -fstack-protector
endif
endif
endif

TRACE_LDFLAGS := -Wl,-z,noexecstack
TRACE_LDFLAGS += -Wl,-z,relro,-z,now
TRACE_LDFLAGS += -pie
TRACE_LDFLAGS += $(LDFLAGS)

all:
	$(CC) -o $(OUT_DIR)/acrntrace acrntrace.c sbuf.c -I. -lpthread -lrt $(TRACE_CFLAGS) $(TRACE_LDFLAGS)

clean:
	rm -f $(OUT_DIR)/acrntrace
ifneq ($(OUT_DIR),.)
	rm -rf $(OUT_DIR)
endif

install: $(OUT_DIR)/acrntrace
	install -d $(DESTDIR)/usr/bin
	install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrntrace

T := $(CURDIR)
OUT_DIR ?= $(shell mkdir -p $(T)/build;cd $(T)/build;pwd)
CC ?= gcc

LIFEMNGR_CFLAGS := -g -std=gnu11
LIFEMNGR_CFLAGS += -D_GNU_SOURCE
LIFEMNGR_CFLAGS += -DNO_OPENSSL
LIFEMNGR_CFLAGS += -m64
LIFEMNGR_CFLAGS += -Wall -ffunction-sections
LIFEMNGR_CFLAGS += -Werror
LIFEMNGR_CFLAGS += -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
LIFEMNGR_CFLAGS += -Wformat -Wformat-security -fno-strict-aliasing
LIFEMNGR_CFLAGS += -fpie -fpic
LIFEMNGR_CFLAGS += $(CFLAGS)

GCC_MAJOR=$(shell echo __GNUC__ | $(CC) -E -x c - | tail -n 1)
GCC_MINOR=$(shell echo __GNUC_MINOR__ | $(CC) -E -x c - | tail -n 1)

#enable stack overflow check
STACK_PROTECTOR := 1

ifdef STACK_PROTECTOR
ifeq (true, $(shell [ $(GCC_MAJOR) -gt 4 ] && echo true))
LIFEMNGR_CFLAGS += -fstack-protector-strong
else
ifeq (true, $(shell [ $(GCC_MAJOR) -eq 4 ] && [ $(GCC_MINOR) -ge 9 ] && echo true))
LIFEMNGR_CFLAGS += -fstack-protector-strong
else
LIFEMNGR_CFLAGS += -fstack-protector
endif
endif
endif

LIFEMNGR_LDFLAGS := -Wl,-z,noexecstack
LIFEMNGR_LDFLAGS += -Wl,-z,relro,-z,now
LIFEMNGR_LDFLAGS += -pie
LIFEMNGR_LDFLAGS += $(LDFLAGS)

all:
	$(CC) -g life_mngr.c -o $(OUT_DIR)/life_mngr -lpthread $(LIFEMNGR_CFLAGS) $(LIFEMNGR_LDFLAGS)
	cp life_mngr.service $(OUT_DIR)/life_mngr.service

	x86_64-w64-mingw32-gcc -g life_mngr_win.c -o $(OUT_DIR)/life_mngr_win.exe -Wall -O2 $(LDFLAGS)
	cp COPYING.MinGW-w64-runtime.txt $(OUT_DIR)/COPYING.MinGW-w64-runtime.txt

clean:
	rm -f $(OUT_DIR)/life_mngr
ifneq ($(OUT_DIR),.)
	rm -f $(OUT_DIR)/life_mngr.service
	rm -rf $(OUT_DIR)
endif


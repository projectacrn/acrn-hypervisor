include ../../../paths.make
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
LIFEMNGR_CFLAGS += -fno-delete-null-pointer-checks -fwrapv
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

# set cross compiler for Windows
MINGWIN_CC := x86_64-w64-mingw32-gcc
MINGWIN_LD := x86_64-w64-mingw32-ld

CONF_DIR = $(sysconfdir)/life_mngr
LM_C_SRCS += monitor.c
LM_C_SRCS += command.c
LM_C_SRCS += uart_channel.c
LM_C_SRCS += uart.c
LM_C_SRCS += socket.c
LM_C_SRCS += command_handler.c
LM_C_SRCS += config.c
all: all-linux all-win

life_mngr.service: life_mngr.service.in
	sed \
	    -e 's|[@]bindir[@]|$(bindir)|g' \
	    < $< > $@-t && \
	    mv $@-t $(OUT_DIR)/$@
all-linux: life_mngr.service
	$(CC) -g $(LM_C_SRCS) -o $(OUT_DIR)/life_mngr -lpthread $(LIFEMNGR_CFLAGS) $(LIFEMNGR_LDFLAGS)
	cp life_mngr.conf $(OUT_DIR)/life_mngr.conf
	cp s5_trigger_linux.py $(OUT_DIR)/s5_trigger_linux.py

# only build for Windows if cross compiler is installed
all-win:
ifeq ($(shell which $(MINGWIN_CC)),)
	@echo "WARN: $(MINGWIN_CC) not installed, skipping life_mngr_win.exe" >&2
else ifneq ($(findstring -z,$(LDFLAGS)),)
	@echo "WARN: $(MINGWIN_LD) does not support -z option, skipping life_mngr_win.exe" >&2
else
	$(MINGWIN_CC) -g life_mngr_win.c -o $(OUT_DIR)/life_mngr_win.exe -Wall -O2 $(LDFLAGS) -lwsock32
	cp COPYING.MinGW-w64-runtime.txt $(OUT_DIR)/COPYING.MinGW-w64-runtime.txt
	cp s5_trigger_win.py $(OUT_DIR)/s5_trigger_win.py
	cp system_reboot_trigger_win.py $(OUT_DIR)/system_reboot_trigger_win.py
endif

clean:
	rm -f $(OUT_DIR)/life_mngr
ifneq ($(OUT_DIR),.)
	rm -f $(OUT_DIR)/life_mngr.service
	rm -f $(OUT_DIR)/life_mngr.conf
	rm -f $(OUT_DIR)/s5_trigger_linux.py
	rm -rf $(OUT_DIR)
endif

.PHONY: install
install:
	install -d $(DESTDIR)$(CONF_DIR)
	install -d $(DESTDIR)$(bindir)
	install -t $(DESTDIR)$(bindir) $(OUT_DIR)/life_mngr
	install -m 0644 -t $(DESTDIR)$(CONF_DIR) $(OUT_DIR)/life_mngr.conf
	install -p -D -m 0644 $(OUT_DIR)/life_mngr.service $(DESTDIR)$(systemd_unitdir)/system/

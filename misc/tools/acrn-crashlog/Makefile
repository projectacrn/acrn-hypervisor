#
# ACRN-Crashlog Makefile
#

BASEDIR 	:= $(shell pwd)
OUT_DIR 	?= $(BASEDIR)
BUILDDIR	:= $(OUT_DIR)/acrn-crashlog
CC		?= gcc
RM		= rm
RELEASE 	?= 0

ifeq ($(RELEASE),0)
CFLAGS	+= -DDEBUG_ACRN_CRASHLOG
endif

CFLAGS := -g -O0 -std=gnu11
CFLAGS += -D_GNU_SOURCE
CFLAGS += -m64
CFLAGS += -Wall -ffunction-sections
CFLAGS += -Werror
CFLAGS += -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
CFLAGS += -Wformat -Wformat-security -fno-strict-aliasing
CFLAGS += -fpie
CFLAGS += -Wall -Wextra -pedantic

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
LDFLAGS += -pie
INCLUDE := -I $(BASEDIR)/common/include

export INCLUDE
export BUILDDIR
export CC
export RM

PKG_CONFIG_PATH := $(PKG_CONFIG_PATH):$(SYSROOT)/usr/lib/pkgconfig
PKG_CONFIG_PATH := $(PKG_CONFIG_PATH):$(SYSROOT)/usr/share/pkgconfig
PKG_CONFIG_PATH := $(PKG_CONFIG_PATH):$(SYSROOT)/usr/local/lib/pkgconfig
PKG_CONFIG_PATH := $(PKG_CONFIG_PATH):$(SYSROOT)/usr/local/share/pkgconfig
PKG_CONFIG_PATH := $(PKG_CONFIG_PATH):$(SYSROOT)/usr/lib32/pkgconfig
PKG_CONFIG_PATH := $(PKG_CONFIG_PATH):$(SYSROOT)/usr/lib64/pkgconfig

EXTRA_LIBS = -lsystemd-journal
PKG_CONFIG := $(shell export PKG_CONFIG_PATH=$(PKG_CONFIG_PATH); \
	pkg-config --libs libsystemd)
LIB_EXIST := $(findstring lsystemd, $(PKG_CONFIG))
ifeq ($(strip $(LIB_EXIST)),lsystemd)
	EXTRA_LIBS := -lsystemd
endif

PKG_CONFIG := $(shell export PKG_CONFIG_PATH=$(PKG_CONFIG_PATH); \
	pkg-config --libs libtelemetry)
LIB_EXIST := $(findstring ltelemetry, $(PKG_CONFIG))
ifeq ($(strip $(LIB_EXIST)),ltelemetry)
        CFLAGS  += -DHAVE_TELEMETRICS_CLIENT
        EXTRA_LIBS += -ltelemetry
endif
export CFLAGS
export LDFLAGS
export EXTRA_LIBS

.PHONY:all
all:
	$(MAKE) -C common
	$(MAKE) -C acrnprobe
	$(MAKE) -C usercrash

.PHONY:clean
clean:
	$(MAKE) -C common clean
	$(MAKE) -C acrnprobe clean
	$(MAKE) -C usercrash clean
	@if [ -d "$(BUILDDIR)" ]; then \
		$(RM) -rf $(BUILDDIR); \
	fi

.PHONY:install
install:
	@install -d $(DESTDIR)/usr/bin/
	@install -p -D -m 0755 $(BUILDDIR)/acrnprobe/bin/acrnprobe $(DESTDIR)/usr/bin/
	@install -p -D -m 0755 $(BUILDDIR)/usercrash/bin/debugger $(DESTDIR)/usr/bin/
	@install -p -D -m 0755 $(BUILDDIR)/usercrash/bin/usercrash_c $(DESTDIR)/usr/bin/
	@install -p -D -m 0755 $(BUILDDIR)/usercrash/bin/usercrash_s $(DESTDIR)/usr/bin/
	@install -p -D -m 0755 data/crashlogctl $(DESTDIR)/usr/bin/
	@install -p -D -m 0755 data/usercrash-wrapper $(DESTDIR)/usr/bin/
	@install -d $(DESTDIR)/usr/share/acrn/crashlog
	@install -p -D -m 0644 data/40-watchdog.conf $(DESTDIR)/usr/share/acrn/crashlog
	@install -p -D -m 0644 data/80-coredump.conf $(DESTDIR)/usr/share/acrn/crashlog
	@install -d $(DESTDIR)/usr/share/defaults/telemetrics/
	@install -p -D -m 0644 data/acrnprobe.xml $(DESTDIR)/usr/share/defaults/telemetrics/
	@install -d $(DESTDIR)/usr/lib/systemd/system/
	@install -p -D -m 0644 data/acrnprobe.service $(DESTDIR)/usr/lib/systemd/system/
	@install -p -D -m 0644 data/usercrash.service $(DESTDIR)/usr/lib/systemd/system/
	@install -d $(DESTDIR)/usr/lib/tmpfiles.d
	@install -p -D -m 0644 data/acrn-crashlog-dirs.conf $(DESTDIR)/usr/lib/tmpfiles.d/

.PHONY:uninstall
uninstall:
	@if [ -e "$(DESTDIR)/usr/bin/acrnprobe" ];then \
		$(RM) $(DESTDIR)/usr/bin/acrnprobe; \
	fi
	@if [ -e "$(DESTDIR)/usr/bin/crashlogctl" ];then \
		$(DESTDIR)/usr/bin/crashlogctl disable && \
		$(RM) $(DESTDIR)/usr/bin/crashlogctl; \
	fi
	@if [ -e "$(DESTDIR)/usr/bin/debugger" ];then \
		$(RM) $(DESTDIR)/usr/bin/debugger; \
	fi
	@if [ -e "$(DESTDIR)/usr/bin/usercrash_c" ];then \
		$(RM) $(DESTDIR)/usr/bin/usercrash_c; \
	fi
	@if [ -e "$(DESTDIR)/usr/bin/usercrash_s" ];then \
		$(RM) $(DESTDIR)/usr/bin/usercrash_s; \
	fi
	@if [ -e "$(DESTDIR)/usr/bin/usercrash-wrapper" ];then \
		$(RM) $(DESTDIR)/usr/bin/usercrash-wrapper; \
	fi
	@if [ -e "$(DESTDIR)/usr/share/acrn/crashlog/40-watchdog.conf" ];then \
		$(RM) $(DESTDIR)/usr/share/acrn/crashlog/40-watchdog.conf; \
	fi
	@if [ -e "$(DESTDIR)/usr/share/acrn/crashlog/80-coredump.conf" ];then \
		$(RM) $(DESTDIR)/usr/share/acrn/crashlog/80-coredump.conf; \
	fi
	@if [ -e "$(DESTDIR)/usr/share/defaults/telemetrics/acrnprobe.xml" ];then \
		$(RM) $(DESTDIR)/usr/share/defaults/telemetrics/acrnprobe.xml; \
	fi
	@if [ -e "$(DESTDIR)/usr/lib/systemd/system/acrnprobe.service" ];then \
		$(RM) $(DESTDIR)/usr/lib/systemd/system/acrnprobe.service; \
	fi
	@if [ -e "$(DESTDIR)/usr/lib/systemd/system/usercrash.service" ];then \
		$(RM) $(DESTDIR)/usr/lib/systemd/system/usercrash.service; \
	fi
	@if [ -e "$(DESTDIR)/usr/lib/tmpfiles.d/acrn-crashlog-dirs.conf" ];then \
		$(RM) $(DESTDIR)/usr/lib/tmpfiles.d/acrn-crashlog-dirs.conf; \
	fi

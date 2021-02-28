#
# ACRN-Crashlog Makefile
#

include ../../../paths.make

BASEDIR 	:= $(shell pwd)
OUT_DIR 	?= $(BASEDIR)
BUILDDIR	:= $(OUT_DIR)/acrn-crashlog
CC		?= gcc
RM		= rm

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

ifeq ($(RELEASE),n)
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
	@install -d $(DESTDIR)$(bindir)/
	@install -p -D -m 0755 $(BUILDDIR)/acrnprobe/bin/acrnprobe $(DESTDIR)$(bindir)/
	@install -p -D -m 0755 $(BUILDDIR)/usercrash/bin/debugger $(DESTDIR)$(bindir)/
	@install -p -D -m 0755 $(BUILDDIR)/usercrash/bin/usercrash_c $(DESTDIR)$(bindir)/
	@install -p -D -m 0755 $(BUILDDIR)/usercrash/bin/usercrash_s $(DESTDIR)$(bindir)/
	@install -p -D -m 0755 data/crashlogctl $(DESTDIR)$(bindir)/
	@install -p -D -m 0755 data/usercrash-wrapper $(DESTDIR)$(bindir)/
	@install -d $(DESTDIR)$(datadir)/acrn/crashlog
	@install -p -D -m 0644 data/40-watchdog.conf $(DESTDIR)$(datadir)/acrn/crashlog
	@install -p -D -m 0644 data/80-coredump.conf $(DESTDIR)$(datadir)/acrn/crashlog
	@install -d $(DESTDIR)$(systemd_unitdir)/system/
	@install -p -D -m 0644 data/acrnprobe.service $(DESTDIR)$(systemd_unitdir)/system/
	@install -p -D -m 0644 data/usercrash.service $(DESTDIR)$(systemd_unitdir)/system/

.PHONY:uninstall
uninstall:
	@if [ -e "$(DESTDIR)$(bindir)/acrnprobe" ];then \
		$(RM) $(DESTDIR)$(bindir)/acrnprobe; \
	fi
	@if [ -e "$(DESTDIR)$(bindir)/crashlogctl" ];then \
		$(DESTDIR)$(bindir)/crashlogctl disable && \
		$(RM) $(DESTDIR)$(bindir)/crashlogctl; \
	fi
	@if [ -e "$(DESTDIR)$(bindir)/debugger" ];then \
		$(RM) $(DESTDIR)$(bindir)/debugger; \
	fi
	@if [ -e "$(DESTDIR)$(bindir)/usercrash_c" ];then \
		$(RM) $(DESTDIR)$(bindir)/usercrash_c; \
	fi
	@if [ -e "$(DESTDIR)$(bindir)/usercrash_s" ];then \
		$(RM) $(DESTDIR)$(bindir)/usercrash_s; \
	fi
	@if [ -e "$(DESTDIR)$(bindir)/usercrash-wrapper" ];then \
		$(RM) $(DESTDIR)$(bindir)/usercrash-wrapper; \
	fi
	@if [ -e "$(DESTDIR)$(datadir)/acrn/crashlog/40-watchdog.conf" ];then \
		$(RM) $(DESTDIR)$(datadir)/acrn/crashlog/40-watchdog.conf; \
	fi
	@if [ -e "$(DESTDIR)$(datadir)/acrn/crashlog/80-coredump.conf" ];then \
		$(RM) $(DESTDIR)$(datadir)/acrn/crashlog/80-coredump.conf; \
	fi
	@if [ -e "$(DESTDIR)$(systemd_unitdir)/system/acrnprobe.service" ];then \
		$(RM) $(DESTDIR)$(systemd_unitdir)/system/acrnprobe.service; \
	fi
	@if [ -e "$(DESTDIR)$(systemd_unitdir)/system/usercrash.service" ];then \
		$(RM) $(DESTDIR)$(systemd_unitdir)/system/usercrash.service; \
	fi

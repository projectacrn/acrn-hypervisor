#
# ACRN-Crashlog Makefile
#

BASEDIR 	:= $(shell pwd)
OUT_DIR 	?= $(BASEDIR)
BUILDDIR	:= $(OUT_DIR)/acrn-crashlog
CC		:= gcc
RM		= rm
RELEASE 	?= 0

CFLAGS	:= -Wall -Wextra -pedantic
CFLAGS	+= -m64 -D_GNU_SOURCE
ifeq ($(RELEASE),0)
CFLAGS	+= -DDEBUG_ACRN_CRASHLOG
endif
INCLUDE := -I $(BASEDIR)/common/include
export INCLUDE
export BUILDDIR
export CC
export RM

EXTRA_LIBS = -lsystemd
LDCNF := $(shell ldconfig -p)
LIB_EXIST = $(findstring libsystemd-journal.so, $(LDCNF))
ifeq ($(strip $(LIB_EXIST)),libsystemd-journal.so)
	EXTRA_LIBS = -lsystemd-journal
endif
LIB_EXIST = $(findstring libtelemetry.so, $(LDCNF))
ifeq ($(strip $(LIB_EXIST)),libtelemetryxx.so)
	CFLAGS	+= -DHAVE_TELEMETRICS_CLIENT
	EXTRA_LIBS += -ltelemetry
endif
export CFLAGS
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
	@install -p -D -m 0755 data/acrnprobe_prepare.sh $(DESTDIR)/usr/bin/
	@install -d $(DESTDIR)/usr/lib/systemd/system.conf.d/
	@install -p -D -m 0644 data/40-watchdog.conf $(DESTDIR)/usr/lib/systemd/system.conf.d/
	@install -d $(DESTDIR)/usr/share/defaults/telemetrics/
	@install -p -D -m 0644 data/acrnprobe.xml $(DESTDIR)/usr/share/defaults/telemetrics/
	@install -d $(DESTDIR)/usr/lib/systemd/system/
	@install -p -D -m 0644 data/acrnprobe.service $(DESTDIR)/usr/lib/systemd/system/
	@install -p -D -m 0644 data/prepare.service $(DESTDIR)/usr/lib/systemd/system/
	@install -p -D -m 0644 data/usercrash.service $(DESTDIR)/usr/lib/systemd/system/

.PHONY:uninstall
uninstall:
	@if [ -e "$(DESTDIR)/usr/bin/acrnprobe" ];then \
		$(RM) $(DESTDIR)/usr/bin/acrnprobe; \
	fi
	@if [ -e "$(DESTDIR)/usr/bin/acrnprobe_prepare.sh" ];then \
		$(RM) $(DESTDIR)/usr/bin/acrnprobe_prepare.sh; \
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
	@if [ -e "$(DESTDIR)/usr/lib/systemd/system.conf.d/40-watchdog.conf" ];then \
		$(RM) $(DESTDIR)/usr/lib/systemd/system.conf.d/40-watchdog.conf; \
	fi
	@if [ -e "$(DESTDIR)/usr/share/defaults/telemetrics/acrnprobe.xml" ];then \
		$(RM) $(DESTDIR)/usr/share/defaults/telemetrics/acrnprobe.xml; \
	fi
	@if [ -e "$(DESTDIR)/usr/lib/systemd/system/acrnprobe.service" ];then \
		$(RM) $(DESTDIR)/usr/lib/systemd/system/acrnprobe.service; \
	fi
	@if [ -e "$(DESTDIR)/usr/lib/systemd/system/prepare.service" ];then \
		$(RM) $(DESTDIR)/usr/lib/systemd/system/prepare.service; \
	fi
	@if [ -e "$(DESTDIR)/usr/lib/systemd/system/usercrash.service" ];then \
		$(RM) $(DESTDIR)/usr/lib/systemd/system/usercrash.service; \
	fi

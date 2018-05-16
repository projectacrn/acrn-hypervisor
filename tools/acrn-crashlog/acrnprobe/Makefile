MAJOR_VERSION=1
MINOR_VERSION=0

BASEDIR 	:= $(shell pwd)

LIBS		= -lpthread -lxml2 -lcrypto -lrt -lsystemd -ltelemetry
INCLUDE		+= -I $(BASEDIR)/include -I /usr/include/libxml2
CFLAGS 		+= $(INCLUDE)
CFLAGS 		+= -g -O0 -std=gnu11
CFLAGS 		+= -ffunction-sections -fdata-sections

LDFLAGS 	+= $(LIBS) -Wl,--gc-sections

TARGET		= $(BUILDDIR)/acrnprobe/bin/acrnprobe

all: include/version.h check_dirs $(TARGET)

$(BUILDDIR)/acrnprobe/obj/%.o:%.c
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILDDIR)/acrnprobe/bin/acrnprobe: $(BUILDDIR)/acrnprobe/obj/main.o \
	$(BUILDDIR)/common/obj/log_sys.o \
	$(BUILDDIR)/common/obj/cmdutils.o \
	$(BUILDDIR)/common/obj/fsutils.o \
	$(BUILDDIR)/common/obj/strutils.o \
	$(BUILDDIR)/acrnprobe/obj/load_conf.o \
	$(BUILDDIR)/acrnprobe/obj/channels.o \
	$(BUILDDIR)/acrnprobe/obj/event_queue.o \
	$(BUILDDIR)/acrnprobe/obj/event_handler.o \
	$(BUILDDIR)/acrnprobe/obj/crash_reclassify.o \
	$(BUILDDIR)/acrnprobe/obj/sender.o \
	$(BUILDDIR)/acrnprobe/obj/startupreason.o \
	$(BUILDDIR)/acrnprobe/obj/property.o \
	$(BUILDDIR)/acrnprobe/obj/probeutils.o \
	$(BUILDDIR)/acrnprobe/obj/history.o \
	$(BUILDDIR)/acrnprobe/obj/android_events.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	@echo "Clean objects and binaries"
	@if [ -e include/version.h ]; then \
		$(RM) -f include/version.h; \
	fi
	@if [ -d $(BUILDDIR)/acrnprobe/obj ]; then \
	    find $(BUILDDIR)/acrnprobe/obj -name "*.o" -exec $(RM) {} \; 2>&1 || exit 0; \
	fi
	@if [ -d $(BUILDDIR)/acrnprobe/bin ]; then \
		$(RM) -r $(BUILDDIR)/acrnprobe/bin ; \
	fi
	@if [ -d $(BUILDDIR)/acrnprobe/obj ]; then \
		$(RM) -r $(BUILDDIR)/acrnprobe/obj ; \
	fi

include/version.h:
	touch include/version.h
	@COMMIT=`git log -1 --pretty=format:%h . 2>/dev/null`;\
	DIRTY=`git diff --name-only $(BASEDIR)`;\
	if [ -n "$$DIRTY" ];then PATCH="$$COMMIT-dirty";else PATCH="$$COMMIT";fi;\
	TIME=`date "+%Y-%m-%d %H:%M:%S"`;\
	cat $(BASEDIR)/../license_header > include/version.h;\
	echo "#define AP_MAJOR_VERSION $(MAJOR_VERSION)" >> include/version.h;\
	echo "#define AP_MINOR_VERSION $(MINOR_VERSION)" >> include/version.h;\
	echo "#define AP_BUILD_VERSION "\""$$PATCH"\""" >> include/version.h;\
	echo "#define AP_BUILD_TIME "\""$$TIME"\""" >> include/version.h;\
	echo "#define AP_BUILD_USER "\""$(USER)"\""" >> include/version.h

check_dirs:
	@if [ ! -d $(BUILDDIR)/acrnprobe/bin ]; then \
		mkdir -p $(BUILDDIR)/acrnprobe/bin ; \
	fi
	@if [ ! -d $(BUILDDIR)/acrnprobe/obj ]; then \
		mkdir -p $(BUILDDIR)/acrnprobe/obj ; \
	fi

MAJOR_VERSION=1
MINOR_VERSION=0

VERSION_H	= $(BUILDDIR)/include/acrnprobe/version.h

LIBS		= -lpthread -lxml2 -lcrypto -lrt -lblkid -lext2fs -lcom_err \
		  $(EXTRA_LIBS)
INCLUDE		+= -I $(CURDIR)/include -I $(SYSROOT)/usr/include/libxml2
INCLUDE		+= -I $(BUILDDIR)/include/acrnprobe
CFLAGS 		+= $(INCLUDE)
CFLAGS 		+= -fdata-sections

LDFLAGS 	+= $(LIBS) -Wl,--gc-sections

TARGET		= $(BUILDDIR)/acrnprobe/bin/acrnprobe

.PHONY: all check_dirs
all: $(VERSION_H) check_dirs $(TARGET)
	rm -f $(VERSION_H)

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
	$(BUILDDIR)/acrnprobe/obj/android_events.o \
	$(BUILDDIR)/acrnprobe/obj/loop.o \
	$(BUILDDIR)/acrnprobe/obj/vmrecord.o
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	@echo "Clean objects and binaries"
	@if [ -e $(VERSION_H) ]; then \
		$(RM) -f $(VERSION_H); \
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

$(VERSION_H):
	@if [ ! -d $(BUILDDIR)/include/acrnprobe ]; then \
		mkdir -p $(BUILDDIR)/include/acrnprobe ; \
	fi
	touch $(VERSION_H)
	@COMMIT=`git log -1 --pretty=format:%h . 2>/dev/null`;\
	DIRTY=`git diff --name-only $(CURDIR)`;\
	if [ -n "$$DIRTY" ];then PATCH="$$COMMIT-dirty";else PATCH="$$COMMIT";fi;\
	TIME=`date "+%Y-%m-%d %H:%M:%S"`;\
	USER=`id -u -n`; \
	cat $(CURDIR)/../license_header > $(VERSION_H);\
	echo "#define AP_MAJOR_VERSION $(MAJOR_VERSION)" >> $(VERSION_H);\
	echo "#define AP_MINOR_VERSION $(MINOR_VERSION)" >> $(VERSION_H);\
	echo "#define AP_BUILD_VERSION "\""$$PATCH"\""" >> $(VERSION_H);\
	echo "#define AP_BUILD_TIME "\""$$TIME"\""" >> $(VERSION_H);\
	echo "#define AP_BUILD_USER "\""$$USER"\""" >> $(VERSION_H)

check_dirs:
	@if [ ! -d $(BUILDDIR)/acrnprobe/bin ]; then \
		mkdir -p $(BUILDDIR)/acrnprobe/bin ; \
	fi
	@if [ ! -d $(BUILDDIR)/acrnprobe/obj ]; then \
		mkdir -p $(BUILDDIR)/acrnprobe/obj ; \
	fi

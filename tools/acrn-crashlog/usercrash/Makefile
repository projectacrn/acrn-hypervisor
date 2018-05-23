MAJOR_VERSION=1
MINOR_VERSION=0

all: include/version.h check_obj usercrash_s usercrash_c debugger

CURRDIR := $(shell pwd)
INCLUDE += -I $(CURRDIR)/include/

LIBS = -levent -lpthread $(EXTRA_LIBS)

usercrash_s: $(BUILDDIR)/usercrash/obj/protocol.o \
	$(BUILDDIR)/usercrash/obj/server.o \
	$(BUILDDIR)/common/obj/log_sys.o
	$(CC) -g $(CFLAGS) $(INCLUDE) $^ -o $(BUILDDIR)/usercrash/bin/$@ $(LIBS)

usercrash_c: $(BUILDDIR)/usercrash/obj/protocol.o \
	$(BUILDDIR)/usercrash/obj/client.o \
	$(BUILDDIR)/usercrash/obj/crash_dump.o \
	$(BUILDDIR)/common/obj/log_sys.o \
	$(BUILDDIR)/common/obj/cmdutils.o \
	$(BUILDDIR)/common/obj/fsutils.o \
	$(BUILDDIR)/common/obj/strutils.o
	$(CC) -g $(CFLAGS) $(INCLUDE) $^ -o $(BUILDDIR)/usercrash/bin/$@ $(LIBS)

debugger: $(BUILDDIR)/usercrash/obj/debugger.o \
	$(BUILDDIR)/usercrash/obj/crash_dump.o \
	$(BUILDDIR)/common/obj/log_sys.o \
	$(BUILDDIR)/common/obj/cmdutils.o \
	$(BUILDDIR)/common/obj/fsutils.o \
	$(BUILDDIR)/common/obj/strutils.o
	$(CC) -g $(CFLAGS) $(INCLUDE) $^ -o $(BUILDDIR)/usercrash/bin/$@ $(LIBS)

$(BUILDDIR)/usercrash/obj/%.o:%.c
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

include/version.h:
	touch include/version.h
	@COMMIT=`git log -1 --pretty=format:%h . 2>/dev/null`;\
	DIRTY=`git diff --name-only $(CURRDIR)`;\
	if [ -n "$$DIRTY" ];then PATCH="$$COMMIT-dirty";else PATCH="$$COMMIT";fi;\
	TIME=`date "+%Y-%m-%d %H:%M:%S"`;\
	cat $(CURRDIR)/../license_header > include/version.h;\
	echo "#define UC_MAJOR_VERSION $(MAJOR_VERSION)" >> include/version.h;\
	echo "#define UC_MINOR_VERSION $(MINOR_VERSION)" >> include/version.h;\
	echo "#define UC_BUILD_VERSION "\""$$PATCH"\""" >> include/version.h;\
	echo "#define UC_BUILD_TIME "\""$$TIME"\""" >> include/version.h;\
	echo "#define UC_BUILD_USER "\""$(USER)"\""" >> include/version.h

check_obj:
	@if [ ! -d $(BUILDDIR)/usercrash/bin ]; then \
		mkdir -p $(BUILDDIR)/usercrash/bin ; \
	fi
	@if [ ! -d $(BUILDDIR)/usercrash/obj ]; then \
		mkdir -p $(BUILDDIR)/usercrash/obj ; \
	fi

.PHONY:clean
clean:
	@echo "Clean objects and binaries"
	@if [ -e include/version.h ]; then \
		$(RM) -f include/version.h; \
	fi
	@if [ -d $(BUILDDIR)/usercrash/obj ]; then \
		find $(BUILDDIR)/usercrash/obj -name "*.o" -exec $(RM) {} \; 2>&1 || exit 0; \
	fi
	@if [ -d $(BUILDDIR)/usercrash/bin ]; then \
		$(RM) -r $(BUILDDIR)/usercrash/bin ; \
	fi
	@if [ -d $(BUILDDIR)/usercrash/obj ]; then \
		$(RM) -r $(BUILDDIR)/usercrash/obj ; \
	fi

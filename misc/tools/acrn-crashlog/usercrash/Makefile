MAJOR_VERSION=1
MINOR_VERSION=0

VERSION_H = $(BUILDDIR)/include/usercrash/version.h

.PHONY: all check_obj
all: $(VERSION_H) check_obj usercrash_s usercrash_c debugger
	rm -f $(VERSION_H)

INCLUDE += -I $(CURDIR)/include/
INCLUDE += -I $(BUILDDIR)/include/usercrash

LIBS = -levent -lpthread $(EXTRA_LIBS)

usercrash_s: $(BUILDDIR)/usercrash/obj/protocol.o \
	$(BUILDDIR)/usercrash/obj/server.o \
	$(BUILDDIR)/common/obj/log_sys.o
	$(CC) -g $(CFLAGS) $(INCLUDE) $^ -o $(BUILDDIR)/usercrash/bin/$@ $(LIBS) $(LDFLAGS)

usercrash_c: $(BUILDDIR)/usercrash/obj/protocol.o \
	$(BUILDDIR)/usercrash/obj/client.o \
	$(BUILDDIR)/usercrash/obj/crash_dump.o \
	$(BUILDDIR)/common/obj/log_sys.o \
	$(BUILDDIR)/common/obj/cmdutils.o \
	$(BUILDDIR)/common/obj/fsutils.o \
	$(BUILDDIR)/common/obj/strutils.o
	$(CC) -g $(CFLAGS) $(INCLUDE) $^ -o $(BUILDDIR)/usercrash/bin/$@ $(LIBS) $(LDFLAGS)

debugger: $(BUILDDIR)/usercrash/obj/debugger.o \
	$(BUILDDIR)/usercrash/obj/crash_dump.o \
	$(BUILDDIR)/common/obj/log_sys.o \
	$(BUILDDIR)/common/obj/cmdutils.o \
	$(BUILDDIR)/common/obj/fsutils.o \
	$(BUILDDIR)/common/obj/strutils.o
	$(CC) -g $(CFLAGS) $(INCLUDE) $^ -o $(BUILDDIR)/usercrash/bin/$@ $(LIBS) $(LDFLAGS)

$(BUILDDIR)/usercrash/obj/%.o:%.c
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

$(VERSION_H):
	@if [ ! -d $(BUILDDIR)/include/usercrash ]; then \
		mkdir -p $(BUILDDIR)/include/usercrash ; \
	fi
	touch $(VERSION_H)
	@COMMIT=`git log -1 --pretty=format:%h . 2>/dev/null`;\
	DIRTY=`git diff --name-only $(CURDIR)`;\
	if [ -n "$$DIRTY" ];then PATCH="$$COMMIT-dirty";else PATCH="$$COMMIT";fi;\
	TIME=`date "+%Y-%m-%d %H:%M:%S"`;\
	USER=`id -u -n`; \
	cat $(CURDIR)/../license_header > $(VERSION_H);\
	echo "#define UC_MAJOR_VERSION $(MAJOR_VERSION)" >> $(VERSION_H);\
	echo "#define UC_MINOR_VERSION $(MINOR_VERSION)" >> $(VERSION_H);\
	echo "#define UC_BUILD_VERSION "\""$$PATCH"\""" >> $(VERSION_H);\
	echo "#define UC_BUILD_TIME "\""$$TIME"\""" >> $(VERSION_H);\
	echo "#define UC_BUILD_USER "\""$$USER"\""" >> $(VERSION_H)

check_obj:
	@if [ ! -d $(BUILDDIR)/usercrash/bin ]; then \
		mkdir -p $(BUILDDIR)/usercrash/bin ; \
	fi
	@if [ ! -d $(BUILDDIR)/usercrash/obj ]; then \
		mkdir -p $(BUILDDIR)/usercrash/obj ; \
	fi

.PHONY: clean
clean:
	@echo "Clean objects and binaries"
	@if [ -e $(VERSION_H) ]; then \
		$(RM) -f $(VERSION_H); \
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

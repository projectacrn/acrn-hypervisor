all: check_obj usercrash_s usercrash_c

CURRDIR := $(shell pwd)
INCLUDE += -I $(CURRDIR)/include/

LIBS = -levent -lpthread

usercrash_s: $(BUILDDIR)/usercrash/obj/protocol.o \
	$(BUILDDIR)/usercrash/obj/server.o \
	$(BUILDDIR)/common/obj/log_sys.o
	$(CC) -g $(CFLAGS) $(LIBS) $(INCLUDE) $^ -o $(BUILDDIR)/usercrash/bin/$@ -lsystemd

usercrash_c: $(BUILDDIR)/usercrash/obj/protocol.o \
	$(BUILDDIR)/usercrash/obj/client.o \
	$(BUILDDIR)/usercrash/obj/crash_dump.o \
	$(BUILDDIR)/common/obj/log_sys.o \
	$(BUILDDIR)/common/obj/cmdutils.o \
	$(BUILDDIR)/common/obj/fsutils.o \
	$(BUILDDIR)/common/obj/strutils.o
	$(CC) -g $(CFLAGS) $(INCLUDE) $^ -o $(BUILDDIR)/usercrash/bin/$@ -lsystemd

$(BUILDDIR)/usercrash/obj/%.o:%.c
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

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
	@if [ -d $(BUILDDIR)/usercrash/obj ]; then \
		find $(BUILDDIR)/usercrash/obj -name "*.o" -exec $(RM) {} \; 2>&1 || exit 0; \
	fi
	@if [ -d $(BUILDDIR)/usercrash/bin ]; then \
		$(RM) -r $(BUILDDIR)/usercrash/bin ; \
	fi
	@if [ -d $(BUILDDIR)/usercrash/obj ]; then \
		$(RM) -r $(BUILDDIR)/usercrash/obj ; \
	fi

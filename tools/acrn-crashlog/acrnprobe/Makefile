BASEDIR 	:= $(shell pwd)

LIBS		= -lpthread -lxml2 -lcrypto -lrt -lsystemd -ltelemetry
CFLAGS 		+= $(INCLUDE)
CFLAGS 		+= -g -O0 -std=gnu11
CFLAGS 		+= -ffunction-sections -fdata-sections

LDFLAGS 	+= $(LIBS) -Wl,--gc-sections

TARGET		= $(BUILDDIR)/acrnprobe/bin/acrnprobe

all: check_dirs $(TARGET)

$(BUILDDIR)/acrnprobe/obj/%.o:%.c
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILDDIR)/acrnprobe/bin/acrnprobe: $(BUILDDIR)/acrnprobe/obj/main.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	@echo "Clean objects and binaries"
	@if [ -d $(BUILDDIR)/acrnprobe/obj ]; then \
	    find $(BUILDDIR)/acrnprobe/obj -name "*.o" -exec $(RM) {} \; 2>&1 || exit 0; \
	fi
	@if [ -d $(BUILDDIR)/acrnprobe/bin ]; then \
		$(RM) -r $(BUILDDIR)/acrnprobe/bin ; \
	fi
	@if [ -d $(BUILDDIR)/acrnprobe/obj ]; then \
		$(RM) -r $(BUILDDIR)/acrnprobe/obj ; \
	fi

check_dirs:
	@if [ ! -d $(BUILDDIR)/acrnprobe/bin ]; then \
		mkdir -p $(BUILDDIR)/acrnprobe/bin ; \
	fi
	@if [ ! -d $(BUILDDIR)/acrnprobe/obj ]; then \
		mkdir -p $(BUILDDIR)/acrnprobe/obj ; \
	fi

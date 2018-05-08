objects = log_sys.o \
	  strutils.o \
	  cmdutils.o \
	  fsutils.o

all: check_obj $(objects)
check_obj:
	@if [ ! -d $(BUILDDIR)/common/obj ]; then \
		mkdir -p $(BUILDDIR)/common/obj/; \
	fi

$(objects):%.o:%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $(BUILDDIR)/common/obj/$@

.PHONY:clean
clean:
	@echo "Clean objects and binaries"
	@if [ -d $(BUILDDIR)/common/obj ]; then \
		find $(BUILDDIR)/common/obj -name "*.o" -exec $(RM) {} \; 2>&1 || exit 0; \
	fi
	@if [ -d $(BUILDDIR)/common/obj ]; then \
		$(RM) -r $(BUILDDIR)/common/obj ; \
	fi

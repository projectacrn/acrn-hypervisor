
# With V=1 means verbose build. Will show full build commands
#
# Without V means silient build. Only show customed message. And we
# could put more focous on warning.
ifdef V
  Q :=
  vecho := @true
  SUBOPT := "V=1"
else
  Q := @
  vecho := @echo
  SUBOPT:= "--no-print-directory"
endif

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
	$(vecho) "--> Build $<"
	$(Q)$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $(BUILDDIR)/common/obj/$@

.PHONY:clean
clean:
	$(vecho) "Clean acrn-crashlog-common"
	$(Q)echo "Clean objects and binaries"
	$(Q)if [ -d $(BUILDDIR)/common/obj ]; then \
		find $(BUILDDIR)/common/obj -name "*.o" -exec $(RM) {} \; 2>&1 || exit 0; \
	fi
	$(Q)if [ -d $(BUILDDIR)/common/obj ]; then \
		$(RM) -r $(BUILDDIR)/common/obj ; \
	fi

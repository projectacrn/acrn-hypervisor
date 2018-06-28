
OUT_DIR ?= .

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

all:
	$(vecho) "Build acrntrace"
	$(Q)$(CC) -o $(OUT_DIR)/acrntrace acrntrace.c sbuf.c -I. -lpthread -lrt

clean:
	$(vecho) "Clean acrntrace"
	$(Q)rm -f $(OUT_DIR)/acrntrace

install: $(OUT_DIR)/acrntrace
	$(vecho) "Install acrntrace"
	$(Q)install -d $(DESTDIR)/usr/bin
	$(Q)install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrntrace

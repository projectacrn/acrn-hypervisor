
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

$(OUT_DIR)/cbc_lifecycle: cbc_lifecycle.c $(TOOLS_OUT)/libacrn-mngr.a
	$(vecho) "--> Build cbc_lifecycle.c"
	$(Q)gcc -o $@ cbc_lifecycle.c -pthread -L$(TOOLS_OUT) -lacrn-mngr

clean:
	$(vecho) "Clean cbc_lifecycle"
	$(Q)rm $(OUT_DIR)/cbc_lifecycle

install: $(OUT_DIR)/cbc_lifecycle cbc_lifecycle.service
	$(vecho) "Install cbc_lifecycle cbc_lifecycle.service"
	$(Q)install -d $(DESTDIR)/usr/bin
	$(Q)install -t $(DESTDIR)/usr/bin $<
	$(Q)install -d $(DESTDIR)/usr/lib/systemd/system/
	$(Q)install -p -m 0644 cbc_lifecycle.service $(DESTDIR)/usr/lib/systemd/system/

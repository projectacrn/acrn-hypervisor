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
	$(vecho) "--> Build cbc_attach.c"
	$(Q)gcc -o $(OUT_DIR)/cbc_attach cbc_attach.c

clean:
	$(vecho) "Clean cbc_attach"
	$(Q)rm -rf $(OUT_DIR)/cbc_attach

install: $(OUT_DIR)/cbc_attach cbc_attach.service
	$(vecho) "Install cbc_attach cbc_attach.service"
	$(Q)install -d $(DESTDIR)/usr/bin
	$(Q)install -t $(DESTDIR)/usr/bin $(OUT_DIR)/cbc_attach
	$(Q)install -d $(DESTDIR)/usr/lib/systemd/system/
	$(Q)install -p -m 0644 cbc_attach.service $(DESTDIR)/usr/lib/systemd/system/

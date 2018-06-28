
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
	$(vecho) "--> Build arcnlog.c"
	$(Q)$(CC) -g acrnlog.c -o $(OUT_DIR)/acrnlog -lpthread
	$(Q)cp acrnlog.service $(OUT_DIR)/acrnlog.service

clean:
	$(vecho) "Clean arcnlog"
	$(Q)rm -f $(OUT_DIR)/acrnlog
ifneq ($(OUT_DIR),.)
	$(Q)rm -f $(OUT_DIR)/acrnlog.service
endif

install: $(OUT_DIR)/acrnlog
	$(vecho) "Instal arcnlog"
	$(Q)install -d $(DESTDIR)/usr/bin
	$(Q)install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrnlog
	$(Q)install -d $(DESTDIR)/usr/lib/systemd/system
	$(Q)install -p -D -m 0644 $(OUT_DIR)/acrnlog.service $(DESTDIR)/usr/lib/systemd/system


OUT_DIR ?= .
SYSTEMD_NETWORKDIR := usr/lib

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
	$(vecho) "Copy acrnbridge script"
	$(Q)cp acrn.netdev $(OUT_DIR)
	$(Q)cp acrn.network $(OUT_DIR)
	$(Q)cp acrn_tap0.netdev $(OUT_DIR)
	$(Q)cp eth.network $(OUT_DIR)

install:
	$(vecho) "Install acrnbridge script"
	$(Q)install -d $(DESTDIR)/$(SYSTEMD_NETWORKDIR)/systemd/network
	$(Q)install -p -D -m 0644 $(OUT_DIR)/acrn.netdev $(DESTDIR)/$(SYSTEMD_NETWORKDIR)/systemd/network/50-acrn.netdev
	$(Q)install -p -D -m 0644 $(OUT_DIR)/acrn.network $(DESTDIR)/$(SYSTEMD_NETWORKDIR)/systemd/network/50-acrn.network
	$(Q)install -p -D -m 0644 $(OUT_DIR)/acrn_tap0.netdev $(DESTDIR)/$(SYSTEMD_NETWORKDIR)/systemd/network/50-acrn_tap0.netdev
	$(Q)install -p -D -m 0644 $(OUT_DIR)/eth.network $(DESTDIR)/$(SYSTEMD_NETWORKDIR)/systemd/network/50-eth.network

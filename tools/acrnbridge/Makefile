
OUT_DIR ?= .
SYSTEMD_NETWORKDIR := usr/lib

all:
	cp acrn.netdev $(OUT_DIR)
	cp acrn.network $(OUT_DIR)
	cp acrn_tap0.netdev $(OUT_DIR)
	cp eth.network $(OUT_DIR)

install:
	install -d $(DESTDIR)/$(SYSTEMD_NETWORKDIR)/systemd/network
	install -p -D -m 0644 $(OUT_DIR)/acrn.netdev $(DESTDIR)/$(SYSTEMD_NETWORKDIR)/systemd/network
	install -p -D -m 0644 $(OUT_DIR)/acrn.network $(DESTDIR)/$(SYSTEMD_NETWORKDIR)/systemd/network
	install -p -D -m 0644 $(OUT_DIR)/acrn_tap0.netdev $(DESTDIR)/$(SYSTEMD_NETWORKDIR)/systemd/network
	install -p -D -m 0644 $(OUT_DIR)/eth.network $(DESTDIR)/$(SYSTEMD_NETWORKDIR)/systemd/network

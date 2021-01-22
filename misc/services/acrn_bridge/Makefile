include ../../../paths.make

OUT_DIR ?= .

all:
	cp acrn.netdev $(OUT_DIR)
	cp acrn.network $(OUT_DIR)
	cp tap0.netdev $(OUT_DIR)
	cp eth.network $(OUT_DIR)

install:
	install -d $(DESTDIR)$(systemd_unitdir)/network
	install -p -D -m 0644 $(OUT_DIR)/acrn.netdev $(DESTDIR)$(systemd_unitdir)/network/50-acrn.netdev
	install -p -D -m 0644 $(OUT_DIR)/acrn.network $(DESTDIR)$(systemd_unitdir)/network/50-acrn.network
	install -p -D -m 0644 $(OUT_DIR)/tap0.netdev $(DESTDIR)$(systemd_unitdir)/network/50-tap0.netdev
	install -p -D -m 0644 $(OUT_DIR)/eth.network $(DESTDIR)$(systemd_unitdir)/network/50-eth.network


OUT_DIR ?= .

$(OUT_DIR)/cbc_lifecycle: cbc_lifecycle.c ../../tools/acrn-manager/acrn_mngr.h
	gcc -o $@ cbc_lifecycle.c -pthread -L$(OUT_DIR)/../../build/tools -lacrn-mngr

clean:
	rm $(OUT_DIR)/cbc_lifecycle

install: $(OUT_DIR)/cbc_lifecycle cbc_lifecycle.service
	install -d $(DESTDIR)/usr/bin
	install -t $(DESTDIR)/usr/bin $<
	install -p -D -m 0644 cbc_lifecycle.service $(DESTDIR)/usr/lib/systemd/system/

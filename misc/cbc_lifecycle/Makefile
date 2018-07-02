
OUT_DIR ?= .

$(OUT_DIR)/cbc_lifecycle: cbc_lifecycle.c $(TOOLS_OUT)/libacrn-mngr.a
	gcc -o $@ cbc_lifecycle.c -pthread -L$(TOOLS_OUT) -lacrn-mngr

clean:
	rm $(OUT_DIR)/cbc_lifecycle

install: $(OUT_DIR)/cbc_lifecycle cbc_lifecycle.service
	install -d $(DESTDIR)/usr/bin
	install -t $(DESTDIR)/usr/bin $<
	install -d $(DESTDIR)/usr/lib/systemd/system/
	install -p -m 0644 cbc_lifecycle.service $(DESTDIR)/usr/lib/systemd/system/

OUT_DIR ?= .

all:
	gcc -o $(OUT_DIR)/cbc_attach cbc_attach.c -lsystemd

clean:
	rm -rf $(OUT_DIR)/cbc_attach

install: $(OUT_DIR)/cbc_attach
	install -d $(DESTDIR)/usr/bin
	install -t $(DESTDIR)/usr/bin $(OUT_DIR)/cbc_attach
	install -p -D -m 0644 cbc_attach.service $(DESTDIR)/usr/lib/systemd/system/

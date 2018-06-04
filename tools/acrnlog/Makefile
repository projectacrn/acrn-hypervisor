
OUT_DIR ?= .

all:
	$(CC) -g acrnlog.c -o $(OUT_DIR)/acrnlog -lpthread
	cp acrnlog.service $(OUT_DIR)/acrnlog.service

clean:
	rm -f $(OUT_DIR)/acrnlog
	rm -f $(OUT_DIR)/acrnlog.service

install: $(OUT_DIR)/acrnlog
	install -d $(DESTDIR)/usr/bin
	install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrnlog
	install -d $(DESTDIR)/usr/lib/systemd/system
	install -p -D -m 0644 $(OUT_DIR)/acrnlog.service $(DESTDIR)/usr/lib/systemd/system

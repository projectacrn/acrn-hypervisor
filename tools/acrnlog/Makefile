
OUT_DIR ?= .
CFLAGS += -fpie
LDFLAGS += -pie

all:
	$(CC) -g acrnlog.c -o $(OUT_DIR)/acrnlog -lpthread $(CFLAGS) $(LDFLAGS)
	cp acrnlog.service $(OUT_DIR)/acrnlog.service

clean:
	rm -f $(OUT_DIR)/acrnlog
ifneq ($(OUT_DIR),.)
	rm -f $(OUT_DIR)/acrnlog.service
endif

install: $(OUT_DIR)/acrnlog
	install -d $(DESTDIR)/usr/bin
	install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrnlog
	install -d $(DESTDIR)/usr/lib/systemd/system
	install -p -D -m 0644 $(OUT_DIR)/acrnlog.service $(DESTDIR)/usr/lib/systemd/system

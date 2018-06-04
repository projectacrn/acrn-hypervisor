
OUT_DIR ?= .

all:
	$(CC) -o $(OUT_DIR)/acrntrace acrntrace.c sbuf.c -I. -lpthread

clean:
	rm -f $(OUT_DIR)/acrntrace

install: $(OUT_DIR)/acrntrace
	install -d $(DESTDIR)/usr/bin
	install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrntrace

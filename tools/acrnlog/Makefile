
OUT_DIR ?= .

all:
	gcc -g acrnlog.c -o $(OUT_DIR)/acrnlog -lpthread

clean:
	rm $(OUT_DIR)/acrnlog

install: $(OUT_DIR)/acrnlog
	install -d $(DESTDIR)/usr/bin
	install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrnlog

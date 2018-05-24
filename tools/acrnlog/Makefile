
include ../../Defs.mk

OUT_DIR ?= .

all:
	$(CC) -g acrnlog.c -o $(OUT_DIR)/acrnlog -lpthread

clean:
	rm $(OUT_DIR)/acrnlog

install: $(OUT_DIR)/acrnlog
	install -D --mode=0755 $(OUT_DIR)/acrnlog $(DESTDIR)$(INSTALL_BINDIR)/acrnlog

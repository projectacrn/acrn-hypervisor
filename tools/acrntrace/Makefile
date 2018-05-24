include ../../Defs.mk

OUT_DIR ?= .

all:
	$(CC) -o $(OUT_DIR)/acrntrace acrntrace.c sbuf.c -I. -lpthread

clean:
	rm $(OUT_DIR)/acrntrace

install: $(OUT_DIR)/acrntrace
	install -D --mode=0755 $(OUT_DIR)/acrntrace $(DESTDIR)$(INSTALL_BINDIR)/acrntrace

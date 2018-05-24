include ../../Defs.mk

OUT_DIR ?= .

all: acrnctl.c
	$(CC) -o $(OUT_DIR)/acrnctl acrnctl.c -I../../devicemodel/include -Wall -g

clean:
	rm -f $(OUT_DIR)/acrnctl

install: $(OUT_DIR)/acrnctl
	install -D --mode=0755 $(OUT_DIR)/acrnctl $(DESTDIR)$(INSTALL_BINDIR)/acrnctl

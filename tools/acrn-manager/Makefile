
OUT_DIR ?= .

all: acrnctl.c
	gcc -o $(OUT_DIR)/acrnctl acrnctl.c -I../../devicemodel/include -Wall -g

clean:
	rm -f $(OUT_DIR)/acrnctl

install: $(OUT_DIR)/acrnctl
	install -d $(DESTDIR)/usr/bin
	install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrnctl


OUT_DIR ?= .

all: $(OUT_DIR)/libacrn-mngr.a $(OUT_DIR)/acrnctl

$(OUT_DIR)/libacrn-mngr.a: acrn_mngr.c acrn_mngr.h
	$(CC) -c acrn_mngr.c -DMNGR_DEBUG -I../../devicemodel/include -Wall -g
	ar -cr $@ acrn_mngr.o

$(OUT_DIR)/acrnctl: acrnctl.c
	$(CC) -o $(OUT_DIR)/acrnctl acrnctl.c -I../../devicemodel/include -Wall -g

clean:
	rm -f $(OUT_DIR)/acrnctl
	rm -f acrn_mngr.o
	rm -f $(OUT_DIR)/libacrn-mngr.a

install: $(OUT_DIR)/acrnctl
	install -d $(DESTDIR)/usr/bin
	install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrnctl


OUT_DIR ?= .

.PHONY: all
all: $(OUT_DIR)/libacrn-mngr.a $(OUT_DIR)/acrnctl

$(OUT_DIR)/libacrn-mngr.a: acrn_mngr.c acrn_mngr.h
	$(CC) -c acrn_mngr.c -DMNGR_DEBUG -I../../devicemodel/include -Wall -g -o $(OUT_DIR)/acrn_mngr.o
	ar -cr $@ $(OUT_DIR)/acrn_mngr.o
	cp ./acrn_mngr.h $(OUT_DIR)/

$(OUT_DIR)/acrnctl: acrnctl.c acrn_mngr.h
	$(CC) -o $(OUT_DIR)/acrnctl acrnctl.c acrn_vm_ops.c -I../../devicemodel/include -L$(TOOLS_OUT) -lacrn-mngr -lpthread -Wall -g

.PHONY: clean
clean:
	rm -f $(OUT_DIR)/acrnctl
	rm -f $(OUT_DIR)/acrn_mngr.o
	rm -f $(OUT_DIR)/libacrn-mngr.a
	rm -f $(OUT_DIR)/acrn_mngr.h

.PHONY: install
install: $(OUT_DIR)/acrnctl
	install -d $(DESTDIR)/usr/bin
	install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrnctl

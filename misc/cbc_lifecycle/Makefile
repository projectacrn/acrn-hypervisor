
OUT_DIR ?= .

$(OUT_DIR)/cbc_lifecycle: cbc_lifecycle.c ../../tools/acrn-manager/acrn_mngr.h
	gcc -o $@ cbc_lifecycle.c -pthread -L$(OUT_DIR)/../../build/tools -lacrn-mngr

clean:
	rm $(OUT_DIR)/cbc_lifecycle

install: $(OUT_DIR)/cbc_lifecycle
	install -d $(DESTDIR)/usr/bin
	install -t $(DESTDIR)/usr/bin $^

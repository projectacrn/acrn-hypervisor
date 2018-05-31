T := $(CURDIR)

.PHONY: all cbc_lifecycle cbc_attach
all: cbc_lifecycle cbc_attach

cbc_lifecycle:
	make -C $(T)/cbc_lifecycle OUT_DIR=$(OUT_DIR)
cbc_attach:
	make -C $(T)/cbc_attach OUT_DIR=$(OUT_DIR)

.PHONY: clean
clean:
	make -C $(T)/cbc_lifecycle clean
	make -C $(T)/cbc_attach clean
	rm -rf $(OUT_DIR)

.PHONY: install
install: cbc_lifecycle-install cbc_attach-install

cbc_lifecycle-install:
	make -C $(T)/cbc_lifecycle OUT_DIR=$(OUT_DIR) install

cbc_attach-install:
	make -C $(T)/cbc_attach OUT_DIR=$(OUT_DIR) install

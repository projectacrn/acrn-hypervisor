T := $(CURDIR)

.PHONY: all cbc_lifecycle
all: cbc_lifecycle

cbc_lifecycle:
	make -C $(T)/cbc_lifecycle OUT_DIR=$(OUT_DIR)

.PHONY: clean
clean:
	make -C $(T)/cbc_lifecycle clean
	rm -rf $(OUT_DIR)

.PHONY: install
install: cbc_lifecycle-install

cbc_lifecycle-install:
	make -C $(T)/cbc_lifecycle OUT_DIR=$(OUT_DIR) install

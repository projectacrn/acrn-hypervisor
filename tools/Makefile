T := $(CURDIR)
OUT_DIR ?= $(T)/build

.PHONY: all acrn-crashlog acrnlog acrn-manager acrntrace
all: acrn-crashlog acrnlog acrn-manager acrntrace

acrn-crashlog:
	make -C $(T)/acrn-crashlog OUT_DIR=$(OUT_DIR) RELEASE=$(RELEASE)

acrnlog:
	make -C $(T)/acrnlog OUT_DIR=$(OUT_DIR)

acrn-manager:
	make -C $(T)/acrn-manager OUT_DIR=$(OUT_DIR)

acrntrace:
	make -C $(T)/acrntrace OUT_DIR=$(OUT_DIR)

.PHONY: clean
clean:
	make -C $(T)/acrn-crashlog OUT_DIR=$(OUT_DIR) clean
	make -C $(T)/acrn-manager OUT_DIR=$(OUT_DIR) clean
	make -C $(T)/acrntrace OUT_DIR=$(OUT_DIR) clean
	make -C $(T)/acrnlog OUT_DIR=$(OUT_DIR) clean
	rm -rf $(OUT_DIR)

.PHONY: install
install: acrn-crashlog-install acrnlog-install acrn-manager-install acrntrace-install

acrn-crashlog-install:
	make -C $(T)/acrn-crashlog OUT_DIR=$(OUT_DIR) install

acrnlog-install:
	make -C $(T)/acrnlog OUT_DIR=$(OUT_DIR) install

acrn-manager-install:
	make -C $(T)/acrn-manager OUT_DIR=$(OUT_DIR) install

acrntrace-install:
	make -C $(T)/acrntrace OUT_DIR=$(OUT_DIR) install

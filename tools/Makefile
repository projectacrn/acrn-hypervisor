T := $(CURDIR)
OUT_DIR ?= $(shell mkdir -p $(T)/build;cd $(T)/build;pwd)
RELEASE ?= 0

.PHONY: all acrn-crashlog acrnlog acrn-manager acrntrace acrnbridge
ifeq ($(RELEASE),0)
all: acrn-crashlog acrnlog acrn-manager acrntrace acrnbridge
else
all: acrn-manager acrnbridge
endif

acrn-crashlog:
	make -C $(T)/acrn-crashlog OUT_DIR=$(OUT_DIR) RELEASE=$(RELEASE)

acrnlog:
	make -C $(T)/acrnlog OUT_DIR=$(OUT_DIR)

acrn-manager:
	make -C $(T)/acrn-manager OUT_DIR=$(OUT_DIR) RELEASE=$(RELEASE)

acrntrace:
	make -C $(T)/acrntrace OUT_DIR=$(OUT_DIR)

acrnbridge:
	make -C $(T)/acrnbridge OUT_DIR=$(OUT_DIR)

.PHONY: clean
clean:
	make -C $(T)/acrn-crashlog OUT_DIR=$(OUT_DIR) clean
	make -C $(T)/acrn-manager OUT_DIR=$(OUT_DIR) clean
	make -C $(T)/acrntrace OUT_DIR=$(OUT_DIR) clean
	make -C $(T)/acrnlog OUT_DIR=$(OUT_DIR) clean
	rm -rf $(OUT_DIR)

.PHONY: install
ifeq ($(RELEASE),0)
install: acrn-crashlog-install acrnlog-install acrn-manager-install acrntrace-install acrnbridge-install
else
install: acrn-manager-install acrnbridge-install
endif

acrn-crashlog-install:
	make -C $(T)/acrn-crashlog OUT_DIR=$(OUT_DIR) install

acrnlog-install:
	make -C $(T)/acrnlog OUT_DIR=$(OUT_DIR) install

acrn-manager-install:
	make -C $(T)/acrn-manager OUT_DIR=$(OUT_DIR) install

acrntrace-install:
	make -C $(T)/acrntrace OUT_DIR=$(OUT_DIR) install

acrnbridge-install:
	make -C $(T)/acrnbridge OUT_DIR=$(OUT_DIR) install

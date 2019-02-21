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
	$(MAKE) -C $(T)/acrn-crashlog OUT_DIR=$(OUT_DIR) RELEASE=$(RELEASE)

acrnlog:
	$(MAKE) -C $(T)/acrnlog OUT_DIR=$(OUT_DIR)

acrn-manager:
	$(MAKE) -C $(T)/acrn-manager OUT_DIR=$(OUT_DIR) RELEASE=$(RELEASE)

acrntrace:
	$(MAKE) -C $(T)/acrntrace OUT_DIR=$(OUT_DIR)

acrnbridge:
	$(MAKE) -C $(T)/acrnbridge OUT_DIR=$(OUT_DIR)

.PHONY: clean
clean:
	$(MAKE) -C $(T)/acrn-crashlog OUT_DIR=$(OUT_DIR) clean
	$(MAKE) -C $(T)/acrn-manager OUT_DIR=$(OUT_DIR) clean
	$(MAKE) -C $(T)/acrntrace OUT_DIR=$(OUT_DIR) clean
	$(MAKE) -C $(T)/acrnlog OUT_DIR=$(OUT_DIR) clean
	rm -rf $(OUT_DIR)

.PHONY: install
ifeq ($(RELEASE),0)
install: acrn-crashlog-install acrnlog-install acrn-manager-install acrntrace-install acrnbridge-install
else
install: acrn-manager-install acrnbridge-install
endif

acrn-crashlog-install:
	$(MAKE) -C $(T)/acrn-crashlog OUT_DIR=$(OUT_DIR) install

acrnlog-install:
	$(MAKE) -C $(T)/acrnlog OUT_DIR=$(OUT_DIR) install

acrn-manager-install:
	$(MAKE) -C $(T)/acrn-manager OUT_DIR=$(OUT_DIR) install

acrntrace-install:
	$(MAKE) -C $(T)/acrntrace OUT_DIR=$(OUT_DIR) install

acrnbridge-install:
	$(MAKE) -C $(T)/acrnbridge OUT_DIR=$(OUT_DIR) install

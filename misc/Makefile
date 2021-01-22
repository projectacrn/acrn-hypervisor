T := $(CURDIR)
OUT_DIR ?= $(shell mkdir -p $(T)/build;cd $(T)/build;pwd)
RELEASE ?= 0

.PHONY: all acrn-crashlog acrnlog acrn-manager acrntrace acrnbridge life_mngr
ifeq ($(RELEASE),0)
all: acrn-crashlog acrnlog acrn-manager acrntrace acrnbridge life_mngr
else
all: acrn-manager acrnbridge life_mngr
endif

acrn-crashlog:
	$(MAKE) -C $(T)/debug_tools/acrn_crashlog OUT_DIR=$(OUT_DIR) RELEASE=$(RELEASE)

acrnlog:
	$(MAKE) -C $(T)/debug_tools/acrn_log OUT_DIR=$(OUT_DIR)

acrn-manager:
	$(MAKE) -C $(T)/services/acrn_manager OUT_DIR=$(OUT_DIR) RELEASE=$(RELEASE)

acrntrace:
	$(MAKE) -C $(T)/debug_tools/acrn_trace OUT_DIR=$(OUT_DIR)

acrnbridge:
	$(MAKE) -C $(T)/services/acrn_bridge OUT_DIR=$(OUT_DIR)

life_mngr:
	$(MAKE) -C $(T)/services/life_mngr OUT_DIR=$(OUT_DIR)

.PHONY: clean
clean:
	$(MAKE) -C $(T)/debug_tools/acrn_crashlog OUT_DIR=$(OUT_DIR) clean
	$(MAKE) -C $(T)/services/acrn_manager OUT_DIR=$(OUT_DIR) clean
	$(MAKE) -C $(T)/debug_tools/acrn_trace OUT_DIR=$(OUT_DIR) clean
	$(MAKE) -C $(T)/debug_tools/acrn_log OUT_DIR=$(OUT_DIR) clean
	$(MAKE) -C $(T)/services/life_mngr OUT_DIR=$(OUT_DIR) clean
	rm -rf $(OUT_DIR)

.PHONY: install
ifeq ($(RELEASE),0)
install: acrn-crashlog-install acrnlog-install acrn-manager-install acrntrace-install acrnbridge-install \
	acrn-life-mngr-install
else
install: acrn-manager-install acrnbridge-install acrn-life-mngr-install
endif

acrn-crashlog-install:
	$(MAKE) -C $(T)/debug_tools/acrn_crashlog OUT_DIR=$(OUT_DIR) install

acrnlog-install:
	$(MAKE) -C $(T)/debug_tools/acrn_log OUT_DIR=$(OUT_DIR) install

acrn-manager-install:
	$(MAKE) -C $(T)/services/acrn_manager OUT_DIR=$(OUT_DIR) install

acrntrace-install:
	$(MAKE) -C $(T)/debug_tools/acrn_trace OUT_DIR=$(OUT_DIR) install

acrnbridge-install:
	$(MAKE) -C $(T)/services/acrn_bridge OUT_DIR=$(OUT_DIR) install

acrn-life-mngr-install:
	$(MAKE) -C $(T)/services/life_mngr OUT_DIR=$(OUT_DIR) install

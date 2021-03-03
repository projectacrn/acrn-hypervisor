T := $(CURDIR)
OUT_DIR ?= $(shell mkdir -p $(T)/build;cd $(T)/build;pwd)

ifndef RELEASE
  override RELEASE := n
else
  # Backward-compatibility for RELEASE=(0|1)
  ifeq ($(RELEASE),1)
    override RELEASE := y
  else
    ifeq ($(RELEASE),0)
      override RELEASE := n
    endif
  endif
endif

SERVICES_OUT ?= $(shell mkdir -p $(OUT_DIR)/services;cd $(OUT_DIR)/services;pwd)
ifeq ($(RELEASE),n)
  DEBUG_OUT ?= $(shell mkdir -p $(OUT_DIR)/debug_tools;cd $(OUT_DIR)/debug_tools;pwd)
endif

.PHONY: all acrn-manager acrnbridge life_mngr acrn-crashlog acrnlog acrntrace
ifeq ($(RELEASE),n)
all: acrn-manager acrnbridge acrn-crashlog acrnlog acrntrace
else
all: acrn-manager acrnbridge
endif

acrn-manager:
	$(MAKE) -C $(T)/services/acrn_manager OUT_DIR=$(SERVICES_OUT) RELEASE=$(RELEASE)

acrnbridge:
	$(MAKE) -C $(T)/services/acrn_bridge OUT_DIR=$(SERVICES_OUT)

life_mngr:
	$(MAKE) -C $(T)/services/life_mngr OUT_DIR=$(SERVICES_OUT)

acrn-crashlog:
	$(MAKE) -C $(T)/debug_tools/acrn_crashlog OUT_DIR=$(DEBUG_OUT) RELEASE=$(RELEASE)

acrnlog:
	$(MAKE) -C $(T)/debug_tools/acrn_log OUT_DIR=$(DEBUG_OUT)

acrntrace:
	$(MAKE) -C $(T)/debug_tools/acrn_trace OUT_DIR=$(DEBUG_OUT)

.PHONY: clean
clean:
	$(MAKE) -C $(T)/services/acrn_manager OUT_DIR=$(SERVICES_OUT) clean
	$(MAKE) -C $(T)/services/life_mngr OUT_DIR=$(SERVICES_OUT) clean
	$(MAKE) -C $(T)/debug_tools/acrn_crashlog OUT_DIR=$(DEBUG_OUT) clean
	$(MAKE) -C $(T)/debug_tools/acrn_trace OUT_DIR=$(DEBUG_OUT) clean
	$(MAKE) -C $(T)/debug_tools/acrn_log OUT_DIR=$(DEBUG_OUT) clean
	rm -rf $(OUT_DIR)

.PHONY: install
ifeq ($(RELEASE),n)
install: acrn-manager-install acrnbridge-install acrn-crashlog-install \
	acrnlog-install acrntrace-install
else
install: acrn-manager-install acrnbridge-install
endif

acrn-manager-install:
	$(MAKE) -C $(T)/services/acrn_manager OUT_DIR=$(SERVICES_OUT) install

acrnbridge-install:
	$(MAKE) -C $(T)/services/acrn_bridge OUT_DIR=$(SERVICES_OUT) install

acrn-life-mngr-install:
	$(MAKE) -C $(T)/services/life_mngr OUT_DIR=$(SERVICES_OUT) install

acrn-crashlog-install:
	$(MAKE) -C $(T)/debug_tools/acrn_crashlog OUT_DIR=$(DEBUG_OUT) install

acrnlog-install:
	$(MAKE) -C $(T)/debug_tools/acrn_log OUT_DIR=$(DEBUG_OUT) install

acrntrace-install:
	$(MAKE) -C $(T)/debug_tools/acrn_trace OUT_DIR=$(DEBUG_OUT) install

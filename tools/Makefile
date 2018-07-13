T := $(CURDIR)
OUT_DIR ?= $(shell mkdir -p $(T)/build;cd $(T)/build;pwd)

# With V=1 means verbose build. Will show full build commands
#
# Without V means silient build. Only show customed message. And we
# could put more focous on warning.
ifdef V
  Q :=
  vecho := @true
  SUBOPT := "V=1"
else
  Q := @
  vecho := @echo
  SUBOPT:= "--no-print-directory"
endif

.PHONY: all acrn-crashlog acrnlog acrn-manager acrntrace acrnbridge
all: acrn-crashlog acrnlog acrn-manager acrntrace acrnbridge

acrn-crashlog:
	$(Q)make -C $(T)/acrn-crashlog $(SUBOPT) OUT_DIR=$(OUT_DIR) RELEASE=$(RELEASE)

acrnlog:
	$(Q)make -C $(T)/acrnlog $(SUBOPT) OUT_DIR=$(OUT_DIR)

acrn-manager:
	$(Q)make -C $(T)/acrn-manager $(SUBOPT) OUT_DIR=$(OUT_DIR) RELEASE=$(RELEASE)

acrntrace:
	$(Q)make -C $(T)/acrntrace $(SUBOPT) OUT_DIR=$(OUT_DIR)

acrnbridge:
	$(Q)make -C $(T)/acrnbridge $(SUBOPT) OUT_DIR=$(OUT_DIR)

.PHONY: clean
clean:
	$(vecho) "cleanup tools"
	$(Q)make -C $(T)/acrn-crashlog $(SUBOPT) OUT_DIR=$(OUT_DIR) clean
	$(Q)make -C $(T)/acrn-manager $(SUBOPT) OUT_DIR=$(OUT_DIR) clean
	$(Q)make -C $(T)/acrntrace $(SUBOPT) OUT_DIR=$(OUT_DIR) clean
	$(Q)make -C $(T)/acrnlog $(SUBOPT) OUT_DIR=$(OUT_DIR) clean
	$(Q)rm -rf $(OUT_DIR)

.PHONY: install
install: acrn-crashlog-install acrnlog-install acrn-manager-install acrntrace-install acrnbridge-install

acrn-crashlog-install:
	$(vecho) "Install acrn-crashlog"
	$(Q)make -C $(T)/acrn-crashlog $(SUBOPT) OUT_DIR=$(OUT_DIR) install

acrnlog-install:
	$(vecho) "Install crashlog"
	$(Q)make -C $(T)/acrnlog $(SUBOPT) OUT_DIR=$(OUT_DIR) install

acrn-manager-install:
	$(vecho) "Install acrn-manager"
	$(Q)make -C $(T)/acrn-manager $(SUBOPT) OUT_DIR=$(OUT_DIR) install

acrntrace-install:
	$(vecho) "Install acrntrace"
	$(Q)make -C $(T)/acrntrace $(SUBOPT) OUT_DIR=$(OUT_DIR) install

acrnbridge-install:
	$(vecho) "Install acrnbridge"
	$(Q)make -C $(T)/acrnbridge $(SUBOPT) OUT_DIR=$(OUT_DIR) install

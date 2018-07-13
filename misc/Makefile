T := $(CURDIR)

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

.PHONY: all cbc_lifecycle cbc_attach
all: cbc_lifecycle cbc_attach

cbc_lifecycle:
	$(Q)make -C $(T)/cbc_lifecycle $(SUBOPT) OUT_DIR=$(OUT_DIR)
cbc_attach:
	$(Q)make -C $(T)/cbc_attach $(SUBOPT) OUT_DIR=$(OUT_DIR)

.PHONY: clean
clean:
	$(vecho) "Clean misc"
	$(Q)make -C $(T)/cbc_lifecycle clean
	$(Q)make -C $(T)/cbc_attach clean
	$(Q)rm -rf $(OUT_DIR)

.PHONY: install
install: cbc_lifecycle-install cbc_attach-install

cbc_lifecycle-install:
	$(Q)make -C $(T)/cbc_lifecycle OUT_DIR=$(OUT_DIR) install

cbc_attach-install:
	$(Q)make -C $(T)/cbc_attach OUT_DIR=$(OUT_DIR) install

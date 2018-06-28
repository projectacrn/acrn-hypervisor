
OUT_DIR ?= .

CFLAGS := -Wall
CFLAGS += -I../../devicemodel/include
ifeq ($(RELEASE),0)
CFLAGS += -g -DMNGR_DEBUG
endif

LDFLAGS := -L$(OUT_DIR)
LDFLAGS += -lacrn-mngr
LDFLAGS +=  -lpthread

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

.PHONY: all
all: $(OUT_DIR)/libacrn-mngr.a $(OUT_DIR)/acrn_mngr.h $(OUT_DIR)/acrnctl

$(OUT_DIR)/libacrn-mngr.a: acrn_mngr.c acrn_mngr.h
	$(vecho) "--> Build $<"
	$(Q)$(CC) $(CFLAGS) -c acrn_mngr.c -o $(OUT_DIR)/acrn_mngr.o
	$(Q)ar -cr $@ $(OUT_DIR)/acrn_mngr.o

ifneq ($(OUT_DIR),.)
$(OUT_DIR)/acrn_mngr.h:
	$(Q)cp ./acrn_mngr.h $(OUT_DIR)/
endif

$(OUT_DIR)/acrnctl: acrnctl.c acrn_mngr.h $(OUT_DIR)/libacrn-mngr.a
	$(vecho) "--> Build `basename $@`"
	$(Q)$(CC) -o $(OUT_DIR)/acrnctl acrnctl.c acrn_vm_ops.c $(CFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	$(vecho) "Clean acrn-manager"
	$(Q)rm -f $(OUT_DIR)/acrnctl
	$(Q)rm -f $(OUT_DIR)/acrn_mngr.o
	$(Q)rm -f $(OUT_DIR)/libacrn-mngr.a
ifneq ($(OUT_DIR),.)
	$(Q)rm -f $(OUT_DIR)/acrn_mngr.h
endif

.PHONY: install
install: $(OUT_DIR)/acrnctl
	$(vecho) "Install acrn-manager"
	$(Q)install -d $(DESTDIR)/usr/bin
	$(Q)install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrnctl


OUT_DIR ?= .

CFLAGS := -Wall
CFLAGS += -I../../devicemodel/include
ifeq ($(RELEASE),0)
CFLAGS += -g -DMNGR_DEBUG
endif

LDFLAGS := -L$(OUT_DIR)
LDFLAGS += -lacrn-mngr
LDFLAGS +=  -lpthread

.PHONY: all
all: $(OUT_DIR)/libacrn-mngr.a $(OUT_DIR)/acrn_mngr.h $(OUT_DIR)/acrnctl

$(OUT_DIR)/libacrn-mngr.a: acrn_mngr.c acrn_mngr.h
	$(CC) $(CFLAGS) -c acrn_mngr.c -o $(OUT_DIR)/acrn_mngr.o
	ar -cr $@ $(OUT_DIR)/acrn_mngr.o

ifneq ($(OUT_DIR),.)
$(OUT_DIR)/acrn_mngr.h:
	cp ./acrn_mngr.h $(OUT_DIR)/
endif

$(OUT_DIR)/acrnctl: acrnctl.c acrn_mngr.h $(OUT_DIR)/libacrn-mngr.a
	$(CC) -o $(OUT_DIR)/acrnctl acrnctl.c acrn_vm_ops.c $(CFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OUT_DIR)/acrnctl
	rm -f $(OUT_DIR)/acrn_mngr.o
	rm -f $(OUT_DIR)/libacrn-mngr.a
ifneq ($(OUT_DIR),.)
	rm -f $(OUT_DIR)/acrn_mngr.h
endif

.PHONY: install
install: $(OUT_DIR)/acrnctl
	install -d $(DESTDIR)/usr/bin
	install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrnctl

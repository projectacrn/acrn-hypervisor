
OUT_DIR ?= .

CFLAGS := -Wall
CFLAGS += -I../../devicemodel/include
CFLAGS += -I../../devicemodel/include/public
CFLAGS += -I../../hypervisor/include
CFLAGS += -fpie

ifeq ($(RELEASE),0)
CFLAGS += -g -DMNGR_DEBUG
endif

LDFLAGS := -L$(OUT_DIR)
LDFLAGS += -lacrn-mngr
LDFLAGS +=  -lpthread
LDFLAGS += -pie

.PHONY: all
all: $(OUT_DIR)/libacrn-mngr.a $(OUT_DIR)/acrn_mngr.h $(OUT_DIR)/acrnctl $(OUT_DIR)/acrnd

$(OUT_DIR)/libacrn-mngr.a: acrn_mngr.c acrn_mngr.h
	$(CC) $(CFLAGS) -c acrn_mngr.c -o $(OUT_DIR)/acrn_mngr.o
	ar -cr $@ $(OUT_DIR)/acrn_mngr.o

ifneq ($(OUT_DIR),.)
$(OUT_DIR)/acrn_mngr.h: ./acrn_mngr.h
	cp ./acrn_mngr.h $(OUT_DIR)/
endif

$(OUT_DIR)/acrnctl: acrnctl.c acrn_mngr.h $(OUT_DIR)/libacrn-mngr.a
	$(CC) -o $(OUT_DIR)/acrnctl acrnctl.c acrn_vm_ops.c $(CFLAGS) $(LDFLAGS)

$(OUT_DIR)/acrnd: acrnd.c $(OUT_DIR)/libacrn-mngr.a
	$(CC) -o $(OUT_DIR)/acrnd acrnd.c acrn_vm_ops.c $(CFLAGS) $(LDFLAGS)
ifneq ($(OUT_DIR),.)
	cp ./acrnd.service $(OUT_DIR)/acrnd.service
endif

.PHONY: clean
clean:
	rm -f $(OUT_DIR)/acrnctl
	rm -f $(OUT_DIR)/acrn_mngr.o
	rm -f $(OUT_DIR)/libacrn-mngr.a
ifneq ($(OUT_DIR),.)
	rm -f $(OUT_DIR)/acrn_mngr.h
	rm -f $(OUT_DIR)/acrnd.service
endif
	rm -f $(OUT_DIR)/acrnd

.PHONY: install
install: $(OUT_DIR)/acrnctl $(OUT_DIR)/acrn_mngr.h $(OUT_DIR)/libacrn-mngr.a
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/lib/systemd/system
	install -d $(DESTDIR)/usr/lib64/
	install -d $(DESTDIR)/usr/include/acrn
	install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrnctl
	install -t $(DESTDIR)/usr/bin $(OUT_DIR)/acrnd
	install -t $(DESTDIR)/usr/lib64/ $(OUT_DIR)/libacrn-mngr.a
	install -t $(DESTDIR)/usr/include/acrn $(OUT_DIR)/acrn_mngr.h
	install -p -D -m 0644 $(OUT_DIR)/acrnd.service $(DESTDIR)/usr/lib/systemd/system

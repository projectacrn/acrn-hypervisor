T := $(CURDIR)
OUT_DIR ?= $(shell mkdir -p $(T)/build;cd $(T)/build/;pwd)

.PHONY: all userapp rtapp
all: userapp histapp rtapp

userapp:
	$(MAKE) -C $(T)/uservm OUT_DIR=$(OUT_DIR)
histapp:
	cp $(T)/uservm/histapp.py $(OUT_DIR)
rtapp:
	$(MAKE) -C $(T)/rtvm OUT_DIR=$(OUT_DIR)

.PHONY: clean

clean:
	rm -rf $(OUT_DIR)


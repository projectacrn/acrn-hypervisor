CC ?= gcc
CXX ?= g++
T := $(CURDIR)
OUT_DIR ?= $(shell mkdir -p $(T)i/../build;cd $(T)/../build;pwd)

CFLAGS = -Wall -Wextra -Wabi=11 -pedantic

LDLIBS = -lrt -pthread

all: userApp histapp

userApp:
	$(CXX) $(CFLAGS) -o $(OUT_DIR)/userApp userApp.cpp ivshmemlib.c $(LDLIBS)
histapp:
	cp $(T)/histapp.py $(OUT_DIR)

clean:
	rm $(OUT_DIR)/userApp
	rm $(OUT_DIR)/histapp.py

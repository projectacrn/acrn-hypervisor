CC = gcc
CXX ?= g++
T := $(CURDIR)
OUT_DIR ?= $(shell mkdir -p $(T)i/../build;cd $(T)/../build;pwd)

CFLAGS = -Wall -Wextra -Wabi=11 -pedantic

LDLIBS = -lrt -pthread

all: rtApp.c
	$(CXX) $(CFLAGS) -o $(OUT_DIR)/rtApp rtApp.c ivshmemlib.c

clean:
	rm $(OUT_DIR)/rtApp

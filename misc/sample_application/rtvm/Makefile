CC = gcc
CP = g++

CFLAGS = -Wall -Wextra -Wabi=11 -pedantic

LDLIBS = -lrt -pthread

all: rtApp.c
	$(CC) $(CFLAGS) -o rtapp rtApp.c ivshmemlib.c

clean:
	rm rtapp

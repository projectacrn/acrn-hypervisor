CC = gcc
CP = g++

CFLAGS = -Wall -Wextra -Wabi=11 -pedantic

LDLIBS = -lrt -pthread

all: userApp.cpp
	$(CP) $(CFLAGS) -o userapp userApp.cpp ivshmemlib.c $(LDLIBS)

clean:
	rm userapp

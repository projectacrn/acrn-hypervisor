
OUT_DIR ?= .

all: acrnctl.c
	gcc -o $(OUT_DIR)/acrnctl acrnctl.c -I../../devicemodel/include -Wall -g

clean:
	rm -f acrnctl

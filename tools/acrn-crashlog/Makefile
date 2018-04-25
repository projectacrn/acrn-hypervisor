#
# ACRN-Crashlog Makefile
#

BASEDIR 	:= $(shell pwd)
OUT_DIR 	?= $(BASEDIR)
BUILDDIR	:= $(OUT_DIR)/acrn-crashlog
CC		:= gcc
RM		= rm

CFLAGS	:= -Wall -Wextra -pedantic
CFLAGS	+= -m64 -D_GNU_SOURCE -DDEBUG_ACRN_CRASHLOG
INCLUDE := -I $(BASEDIR)/common/include
export INCLUDE
export BUILDDIR

.PHONY:all
all:
	$(MAKE) -C common
	$(MAKE) -C acrnprobe
	$(MAKE) -C usercrash

.PHONY:clean
clean:
	$(MAKE) -C common clean
	$(MAKE) -C acrnprobe clean
	$(MAKE) -C usercrash clean
	@if [ -d "$(BUILDDIR)" ]; then \
		$(RM) -rf $(BUILDDIR); \
	fi

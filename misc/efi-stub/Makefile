#
# Copyright (c) 2011, Intel Corporation
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
#      copyright notice, this list of conditions and the following
#      disclaimer in the documentation and/or other materials provided
#      with the distribution.
#    * Neither the name of Intel Corporation nor the names of its
#      contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

HV_OBJDIR:=build
HV_FILE:=acrn
HV_SRC:=../../hypervisor
C_SRCS = boot.c pe.c malloc.c
ACRN_OBJS := $(patsubst %.c,$(EFI_OBJDIR)/%.o,$(C_SRCS))
INCLUDE_PATH += $(INCDIR)/efi
INCLUDE_PATH += $(INCDIR)/efi/$(ARCH)
INCLUDE_PATH += $(HV_OBJDIR)/include
INCLUDE_PATH += $(HV_SRC)/include/arch/x86
INCLUDE_PATH += $(HV_SRC)/include/arch/x86/guest
INCLUDE_PATH += $(HV_SRC)/include/public
INCLUDE_PATH += $(HV_SRC)/include/lib
INCLUDE_PATH += $(HV_SRC)/boot/include/guest

SCENARIO ?= sdc

OBJCOPY=objcopy

HOST  = $(shell $(CC) -dumpmachine | sed "s/\(-\).*$$//")
ARCH := $(shell $(CC) -dumpmachine | sed "s/\(-\).*$$//")

ifeq ($(ARCH),x86_64)
	FORMAT=efi-app-x86-64
else
	ARCH=ia32
	FORMAT=efi-app-ia32
endif

# Different Linux distributions have the 'gnu-efi' package install
# its tools and libraries in different folders. The next couple of
# variables will determine and set the right path for both the
# tools $(GNUEFI_DIR) and libraries $(LIBDIR)
GNUEFI_DIR := $(shell find $(SYSROOT)/usr/lib* -name elf_$(ARCH)_efi.lds -type f | xargs dirname)
LIBDIR := $(subst gnuefi,,$(GNUEFI_DIR))
CRT0 := $(GNUEFI_DIR)/crt0-efi-$(ARCH).o
LDSCRIPT := $(GNUEFI_DIR)/elf_$(ARCH)_efi.lds

INCDIR := $(SYSROOT)/usr/include

CFLAGS=-I. -I.. -I$(INCDIR)/efi -I$(INCDIR)/efi/$(ARCH) \
		-DEFI_FUNCTION_WRAPPER -fPIC -fshort-wchar -ffreestanding \
		-Wall -I../fs/ -D$(ARCH) -O2	\
		-include config.h

CFLAGS += -mno-mmx -mno-sse -mno-sse2 -mno-80387 -mno-fp-ret-in-387

CFLAGS += -fno-delete-null-pointer-checks -fwrapv
ifeq ($(ARCH),ia32)
	ifeq ($(HOST),x86_64)
		CFLAGS += -m32
	endif
endif
ifeq ($(ARCH),x86_64)
	CFLAGS += -mno-red-zone
endif

LDFLAGS=-T $(LDSCRIPT) -Bsymbolic -shared -nostdlib -znocombreloc \
		-L$(LIBDIR) $(CRT0)
EFIBIN=$(HV_OBJDIR)/$(HV_FILE).efi
BOOT=$(EFI_OBJDIR)/boot.efi
HV_BIN=$(HV_OBJDIR)/$(HV_FILE).bin

CONF_FILE=$(CURDIR)/clearlinux/acrn.conf

all: $(EFIBIN)

install: $(EFIBIN) install-conf
	install -D $(EFIBIN) $(DESTDIR)/usr/lib/acrn/$(HV_FILE).$(BOARD).$(SCENARIO).efi

install-debug: $(HV_OBJDIR)/$(HV_FILE).map $(HV_OBJDIR)/$(HV_FILE).out
	install -D $(HV_OBJDIR)/$(HV_FILE).out $(DESTDIR)/usr/lib/acrn/$(HV_FILE).$(BOARD).$(SCENARIO).efi.out
	install -D $(HV_OBJDIR)/$(HV_FILE).map $(DESTDIR)/usr/lib/acrn/$(HV_FILE).$(BOARD).$(SCENARIO).efi.map

$(EFIBIN): $(BOOT) $(HV_BIN)
	$(OBJCOPY) --add-section .hv="$(HV_BIN)"  --change-section-vma .hv=0x6e000 --set-section-flags .hv=alloc,data,contents,load  --section-alignment 0x1000 $(BOOT) $(EFIBIN)

$(BOOT): $(EFI_OBJDIR)/boot.so

$(EFI_OBJDIR)/boot.so: $(ACRN_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^  -lgnuefi -lefi $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)

install-conf: $(CONF_FILE)
	install -D --mode=0644 $^ $(DESTDIR)/usr/share/acrn/samples/nuc/acrn.conf

clean:
	rm -f $(BOOT) $(HV_OBJDIR)/$(HV_FILE).efi $(EFI_OBJDIR)/boot.so $(ACRN_OBJS)

-include $(ACRN_OBJS:.o=.d)

$(EFI_OBJDIR)/%.o:%.S
	[ ! -e $@ ] && mkdir -p $(dir $@); \
	$(CC) $(CFLAGS)   -c -o $@ $< -MMD -MT $@

$(EFI_OBJDIR)/%.o: %.c
	[ ! -e $@ ] && mkdir -p $(dir $@); \
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) -I. -c $(CFLAGS) $(ARCH_CFLAGS) $< -o $@ -MMD -MT $@

%.efi: %.so
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel \
		-j .rela -j .reloc --target=$(FORMAT) $*.so $@

BASEDIR ?= ..

include $(BASEDIR)/vmcfg/scripts/kconfig/kconfig.mk

all:

install:
	install -d $(DESTDIR)/usr/share/acrn/samples/apl-mrb
	install -t $(DESTDIR)/usr/share/acrn/samples/apl-mrb $(BASEDIR)/vmcfg/apl-mrb/mrb-env-setup.sh

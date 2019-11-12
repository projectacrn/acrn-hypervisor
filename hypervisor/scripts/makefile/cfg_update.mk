CONFIG_XML_ENABLED ?=
UPDATE_RESULT = $(HV_OBJDIR)/.cfg_src_result

define check_xml_enabled =
CONFIG_XML_ENABLED := $(shell if [ "$(1)" != "" ] || [ "$(2)" != "" ]; then echo false; fi)
BOARD_IN_XML := $(shell echo `if [ "$(1)" != "" ]; then sed -n '/<acrn-config/p' $(1) | sed -r 's/.*board="(.*)".*/\1/g'; fi`)
SCENARIO_IN_XML := $(shell echo `if [ "$(2)" != "" ]; then sed -n '/<acrn-config/p' $(2) | sed -r 's/.*scenario="(.*)".*/\1/g'; fi`)

ifneq ($$(BOARD_IN_XML),)
    ifneq ($$(SCENARIO_IN_XML),)
        CONFIG_XML_ENABLED := true
    endif
endif

ifeq ($$(CONFIG_XML_ENABLED),true)
    override BOARD := $$(BOARD_IN_XML)
    override SCENARIO := $$(SCENARIO_IN_XML)
endif


endef

ifeq ($(CONFIG_XML_ENALBED),)
    $(eval $(call check_xml_enabled,$(BOARD_FILE),$(SCENARIO_FILE)))
endif

update_config:
ifeq ($(CONFIG_XML_ENABLED),true)
	@if [ ! -f $(UPDATE_RESULT) ]; then \
		mkdir -p $(dir $(UPDATE_RESULT));\
		python3 ../misc/acrn-config/board_config/board_cfg_gen.py --board $(BOARD_FILE) --scenario $(SCENARIO_FILE) > $(UPDATE_RESULT);\
		cat $(UPDATE_RESULT);\
		if [ "`sed -n /successfully/p $(UPDATE_RESULT)`" = "" ]; then rm -f $(UPDATE_RESULT); exit 1;	fi;\
		python3 ../misc/acrn-config/scenario_config/scenario_cfg_gen.py --board $(BOARD_FILE) --scenario $(SCENARIO_FILE) > $(UPDATE_RESULT);\
		cat $(UPDATE_RESULT);\
		if [ "`sed -n /successfully/p $(UPDATE_RESULT)`" = "" ]; then rm -f $(UPDATE_RESULT); exit 1;	fi;\
		echo "Import hypervisor Board/VM configuration from XMLs, configurations in source code has been overwritten!";\
	elif [ "`sed -n /successfully/p $(UPDATE_RESULT)`" = "" ]; then \
		echo "Problem is found on Board/VM configration patching, please rebuild."; rm -f $(UPDATE_RESULT); exit 1; \
	else \
		echo "Configurations is patched already!";\
	fi;
else ifeq ($(CONFIG_XML_ENABLED),false)
	@echo "Config XML file does not exist or with unknown format."
	@exit 1
else
	@echo "Using hypervisor configurations from source code directly."
endif

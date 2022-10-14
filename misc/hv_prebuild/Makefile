HV_OBJDIR ?= $(CURDIR)/build
HV_CONFIG_H := $(HV_OBJDIR)/include/config.h
HV_SRC_DIR := ../../hypervisor

ifneq ($(HV_CONFIG_H), $(wildcard $(HV_CONFIG_H)))
    $(error $(HV_CONFIG_H) does not exist)
endif

ifeq ($(BOARD),)
    $(error please specify BOARD for the build!)
endif

ifeq ($(SCENARIO),)
    $(error please specify SCENARIO for the build!)
endif

ifeq ($(CHECKER_OUT),)
    $(error please specify the path to the generated checker! )
endif

BOARD_INFO_DIR := $(HV_OBJDIR)/configs/boards
SCENARIO_CFG_DIR := $(HV_OBJDIR)/configs/scenarios/$(SCENARIO)
BOARD_CFG_DIR := $(SCENARIO_CFG_DIR)

PRE_BUILD_SRCS += main.c
PRE_BUILD_SRCS += static_checks.c
PRE_BUILD_SRCS += vm_cfg_checks.c
PRE_BUILD_SRCS += $(HV_SRC_DIR)/arch/x86/configs/vm_config.c
PRE_BUILD_SRCS += $(SCENARIO_CFG_DIR)/vm_configurations.c
PRE_BUILD_SRCS += $(BOARD_CFG_DIR)/pt_intx.c
ifneq (,$(wildcard $(BOARD_CFG_DIR)/pci_dev.c))
PRE_BUILD_SRCS += $(BOARD_CFG_DIR)/pci_dev.c
endif
PRE_BUILD_CFLAGS += -fno-stack-protector -fno-builtin -W -Wall
PRE_BUILD_INCLUDE := $(patsubst %, -I %, $(INCLUDE_PATH)) -include $(HV_CONFIG_H) -I .

.PHONY: default
default: $(PRE_BUILD_SRCS)
	@echo "SCENARIO <$(SCENARIO)> for BOARD <$(BOARD)> is specified."
	@if [ ! -d $(BOARD_INFO_DIR) ]; then \
		echo "Information of BOARD $(BOARD) is not found."; exit 1; \
	else \
		echo "Found BOARD $(BOARD) information under $(BOARD_INFO_DIR)"; \
	fi;
	@if [ ! -d $(SCENARIO_CFG_DIR) ]; then \
		echo "Configurations for SCENARIO $(SCENARIO) is not found."; exit 1; \
	else \
		echo "Found SCENARIO $(SCENARIO) configurations under $(SCENARIO_CFG_DIR)"; \
	fi;
	@if [ ! -d $(BOARD_CFG_DIR) ]; then \
		echo "$(BOARD) configuration for SCENARIO $(SCENARIO) is not found."; exit 1; \
	else \
		echo "Found $(BOARD) configuration for SCENARIO $(SCENARIO) under $(BOARD_CFG_DIR)"; \
	fi;
	$(CC) $(PRE_BUILD_SRCS) $(PRE_BUILD_INCLUDE) $(PRE_BUILD_CFLAGS) -o $(CHECKER_OUT)

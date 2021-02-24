# Copyright (C) 2021 Intel Corporation. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause

# usage: check_coexistence <symbol 1> <symbol 2>
#
# This macro checks if the given symbols are both defined or both not. This is used to check the coexistence of BOARD &
# SCENARIO or BOARD_FILE & SCENARIO_FILE.
define check_coexistence =
ifdef $(1)
  ifndef $(2)
    $$(warning $(1) is defined while $(2) is empty)
    $$(error $(1) & $(2) must be given in pair)
  endif
else
  ifdef $(2)
    $$(warning $(2) is defined while $(1) is empty)
    $$(error $(1) & $(2) must be given in pair)
  endif
endif
endef

# usage: determine_config <symbol> <default>
#
# BOARD and SCENARIO can be specified in the following places.
#
#   1. Configuration data in config.mk (named CONFIG_BOARD or CONFIG_SCENARIO) which are extracted from the existing XML
#      file.
#
#   2. Variables defined on the command line when invoking 'make'.
#
#   3. Default values in this script.
#
# In place #2 it can be either a board/scenario name or path to an XML file. Depending on where these variables are
# defined and their values, this script behaves as follows.
#
#   * If a variable is defined in both #1 and #2 with the same value, that value is effective for the build.
#
#   * If a variable is defined in both #1 and #2 with different values, the build terminates with an error message.
#
#   * If a variable is defined in either #1 or #2 (but not both), that value is effective for the build.
#
#   * If a variable is defined in neither #1 nor #2, the default value is effective for the build.
#
# If #2 gives a path to an XML file, the board/scenario defined in that file is used for the comparison above.
#
# This macro implements the policy. The following variables will be rewritten after the rules are evaluated without any
# error.
#
#   * The <symbol> (i.e. either BOARD or SCENARIO) will always hold the name of the effective board/scenario.
#
#   * The <symbol>_FILE (i.e. either BOARD_FILE or SCENARIO_FILE) will always hold the path to an existing XML file that
#     defines the effective board/scenario. If only a BOARD/SCENARIO name is given, a predefined configuration under
#     misc/config_tools/data/$BOARD will be used.
#
define determine_config =
ifneq ($($(1)),)
  ifneq ($(realpath $($(1))),)
    override $(1)_FILE := $($(1))
    override $(1) := $$(shell xmllint --xpath 'string(/acrn-config/@$(shell echo $(1) | tr A-Z a-z))' $$($(1)_FILE))
  else
    override $(1)_FILE := $(HV_PREDEFINED_DATA_DIR)/$$(BOARD)/$$($(1)).xml
    ifeq ($$(realpath $$($(1)_FILE)),)
      $$(error $(1) = '$($(1))' is neither path to a file nor name of a predefined board)
    endif
  endif
  ifdef CONFIG_$(1)
    ifneq ($$($(1)), $(CONFIG_$(1)))
      $$(warning The command line sets $(1) to be '$$($(1))', but an existing build is configured with '$(CONFIG_$(1))')
      $$(warning Try cleaning up the existing build with 'make clean' or setting HV_OBJDIR to a newly-created directory)
      $$(error Configuration conflict identified)
    endif
  endif
else
  ifdef CONFIG_$(1)
    override $(1) := $(CONFIG_$(1))
  else
    override $(1) := $(2)
  endif
  override $(1)_FILE := $(HV_PREDEFINED_DATA_DIR)/$$(BOARD)/$$($(1)).xml
endif
endef

# usage: determine_build_type <default>
#
# Similar to BOARD or SCENARIO, the RELEASE variable has three sources where it can be defined. But unlike those
# variables that cannot be modified once configured, users shall be able to tweak RELEASE from the command line even a
# build directory has already been configured.
#
# This macro implements the same check as determine_config, but attempts to update the configuration file if RELEASE is
# changed.
define determine_build_type =
ifdef RELEASE
  ifdef CONFIG_RELEASE
    ifneq ($(RELEASE),$(CONFIG_RELEASE))
      $$(warning The command line sets RELEASE to be '$(RELEASE)', but an existing build is configured with '$(CONFIG_RELEASE)')
      $$(warning The configuration will be modified for RELEASE=$(RELEASE))
      $$(shell sed -i "s@\(<RELEASE.*>\)[yn]\(</RELEASE>\)@\1$(RELEASE)\2@g" $(HV_SCENARIO_XML))
    endif
  endif
else
  ifdef CONFIG_RELEASE
    RELEASE := $(CONFIG_RELEASE)
  else
    RELEASE := $(1)
  endif
endif
endef

# Paths to the inputs
HV_BOARD_XML := $(HV_OBJDIR)/.board.xml
HV_SCENARIO_XML := $(HV_OBJDIR)/.scenario.xml
HV_UNIFIED_XML_IN := $(BASEDIR)/scripts/makefile/unified.xml.in
HV_PREDEFINED_DATA_DIR := $(realpath $(BASEDIR)/../misc/config_tools/data)
HV_CONFIG_TOOL_DIR := $(realpath $(BASEDIR)/../misc/config_tools)
HV_CONFIG_XFORM_DIR := $(HV_CONFIG_TOOL_DIR)/xforms

# Paths to the outputs:
HV_CONFIG_DIR := $(HV_OBJDIR)/configs
HV_ALLOCATION_XML := $(HV_CONFIG_DIR)/allocation.xml
HV_UNIFIED_XML := $(HV_CONFIG_DIR)/unified.xml
HV_CONFIG_H := $(HV_OBJDIR)/include/config.h
HV_CONFIG_MK := $(HV_CONFIG_DIR)/config.mk
HV_CONFIG_TIMESTAMP := $(HV_CONFIG_DIR)/.timestamp
HV_DIFFCONFIG_LIST := $(HV_CONFIG_DIR)/.diffconfig

HV_CONFIG_A_DIR := $(HV_OBJDIR)/a            # Directory containing generated configuration sources for diffconfig
HV_CONFIG_B_DIR := $(HV_OBJDIR)/b            # Directory containing edited configuration sources for diffconfig
HV_CONFIG_DIFF := $(HV_OBJDIR)/config.patch  # Patch encoding differences between generated and edited config. sources

# Backward-compatibility for RELEASE=(0|1)
ifdef RELEASE
  ifeq ($(RELEASE),1)
    override RELEASE := y
  else
    ifeq ($(RELEASE),0)
      override RELEASE := n
    endif
  endif
endif

ifeq ($(findstring $(MAKECMDGOALS),distclean),)
-include $(HV_CONFIG_MK)
endif

# BOARD/SCENARIO/BOARD_FILE/SCENARIO_FILE parameters sanity check.
#
# 1. Raise an error if BOARD/SCENARIO (or BOARD_FILE/SCENARIO_FILE) are partially given.
# 2. Raise an error if BOARD_FILE/SCENARIO_FILE are given but do not point to valid XMLs.

$(eval $(call check_coexistence,BOARD,SCENARIO))
$(eval $(call check_coexistence,BOARD_FILE,SCENARIO_FILE))

# BOARD_FILE/SCENARIO_FILE are to be obsoleted. Users can now use BOARD/SCENARIO to give either pre-defined
# board/scenario or their own XML files.
#
# The following block converts BOARD_FILE/SCENARIO_FILE to BOARD/SCENARIO. Removing support of BOARD_FILE/SCENARIO_FILE
# can be done by simply deleting it.
ifneq ($(BOARD_FILE)$(SCENARIO_FILE),)
  $(warning BOARD_FILE/SCENARIO_FILE are obsoleted. Use BOARD/SCENARIO to specify either predefined board/scenario or your own board/scenario XMLs)

  ifneq ($(BOARD)$(SCENARIO),)
    $(warning BOARD/SCENARIO and BOARD_FILE/SCENARIO_FILE are both given. BOARD/SCENARIO will take precedence)
  else
    ifneq ($(BOARD_FILE), $(wildcard $(BOARD_FILE)))
      $(error BOARD_FILE: $(BOARD_FILE) does not exist)
    endif
    ifneq ($(SCENARIO_FILE), $(wildcard $(SCENARIO_FILE)))
      $(error SCENARIO_FILE: $(SCENARIO_FILE) does not exist)
    endif
    BOARD := $(BOARD_FILE)
    SCENARIO := $(SCENARIO_FILE)
  endif
endif

# Internally we still use BOARD to represent the name of the target board and BOARD_FILE to be the path to the board XML
# file. SCENARIO/SCENARIO_FILE are used in the same way. The following block translates the user-visible BOARD/SCENARIO
# (which is multiplexed) to the internal representation.

$(eval $(call determine_config,BOARD,nuc7i7dnb))
$(eval $(call determine_config,SCENARIO,industry))
$(eval $(call determine_build_type,y))

$(HV_BOARD_XML):
	@if [ ! -f $(HV_BOARD_XML) ]; then \
	  if [ -f $(BOARD_FILE) ]; then \
	    echo "Board XML is fetched from $(realpath $(BOARD_FILE))"; \
	    mkdir -p $(dir $(HV_BOARD_XML)); \
	    cp $(BOARD_FILE) $(HV_BOARD_XML); \
	  else \
	    echo "No pre-defined board info available at $(BOARD_FILE)"; \
	    echo "Try setting another predefined BOARD or SCENARIO or specifying a board XML file"; \
	    exit 1; \
	  fi; \
	fi

$(HV_SCENARIO_XML):
	@if [ ! -f $(HV_SCENARIO_XML) ]; then \
	  if [ -f $(SCENARIO_FILE) ]; then \
	    echo "Scneario XML is configuration fetched from $(realpath $(SCENARIO_FILE))"; \
	    mkdir -p $(dir $(HV_SCENARIO_XML)); \
	    cp $(SCENARIO_FILE) $(HV_SCENARIO_XML); \
	  else \
	    echo "No pre-defined scenario available at $(SCENARIO_FILE)"; \
	    echo "Try setting another predefined BOARD or SCENARIO or specifying a scenario XML file"; \
	    exit 1; \
	  fi; \
	fi

# A unified XML is generated to include board and scenario XML files so that XSLT scripts have access to both for
# generating source files.
$(HV_ALLOCATION_XML): $(HV_BOARD_XML) $(HV_SCENARIO_XML) | $(HV_CONFIG_DIR)
	@python3 $(HV_CONFIG_TOOL_DIR)/static_allocators/main.py --board $(HV_BOARD_XML) --scenario $(HV_SCENARIO_XML) --output $(HV_ALLOCATION_XML)
	@echo "$@ generated"

$(HV_UNIFIED_XML): $(HV_BOARD_XML) $(HV_SCENARIO_XML) $(HV_ALLOCATION_XML) | $(HV_CONFIG_DIR)
	@sed "s@{BOARD_FILE}@$(realpath $(HV_BOARD_XML))@g" $(HV_UNIFIED_XML_IN) | \
	 sed "s@{SCENARIO_FILE}@$(HV_SCENARIO_XML)@g" | \
	 sed "s@{ALLOCATION_FILE}@$(HV_ALLOCATION_XML)@g" > $@
	@echo "$@ generated"

$(HV_CONFIG_MK): $(HV_UNIFIED_XML) | $(HV_CONFIG_DIR)
	@xsltproc -o $@ --xinclude --xincludestyle $(HV_CONFIG_XFORM_DIR)/config.mk.xsl $<
	@echo "$@ generated"

$(HV_CONFIG_H): $(HV_UNIFIED_XML)
	@mkdir -p $(dir $(HV_CONFIG_H))
	@xsltproc -o $@ --xinclude --xincludestyle $(HV_CONFIG_XFORM_DIR)/config.h.xsl $<
	@echo "$@ generated"

$(HV_CONFIG_TIMESTAMP): $(HV_UNIFIED_XML) ${HV_DIFFCONFIG_LIST} | $(HV_CONFIG_DIR)
	@sh $(BASEDIR)/scripts/genconf.sh $(BASEDIR) $(HV_BOARD_XML) $(HV_SCENARIO_XML) $(HV_CONFIG_DIR)
	@touch $@

.PHONY: defconfig
defconfig: $(HV_CONFIG_TIMESTAMP)

showconfig:
	@echo "Build directory: $(HV_OBJDIR)"
	@echo "This build directory is configured with the settings below."
	@echo "- BOARD = $(BOARD)"
	@echo "- SCENARIO = $(SCENARIO)"
	@echo "- RELEASE = $(RELEASE)"

diffconfig:
	@rm -rf $(HV_CONFIG_A_DIR) $(HV_CONFIG_B_DIR)
	@sh $(BASEDIR)/scripts/genconf.sh $(BASEDIR) $(BOARD_FILE) $(HV_CONFIG_XML) $(HV_CONFIG_A_DIR)
	@cd $(HV_CONFIG_DIR) && find . -name '*.c' -or -name '*.h' -or -name '*.config' | while read f; do \
	  nf=$(HV_CONFIG_B_DIR)/$${f}; mkdir -p `dirname $${nf}` && cp $${f} $${nf}; \
	done
	@cd $(HV_OBJDIR) && git diff --no-index --no-prefix a/ b/ > $(HV_CONFIG_DIFF) || true
	@echo "Diff on generated configuration files is available at $(HV_CONFIG_DIFF)."
	@echo "To make a patch effective, use `applydiffconfig PATCH=/path/to/patch` to register it to a build."

applydiffconfig:
ifdef PATCH
  ifneq ($(realpath $(PATCH)),)
	@echo $(realpath $(PATCH)) >> ${HV_DIFFCONFIG_LIST}
	@echo "${PATCH} is registered for build directory ${HV_OBJDIR}."
	@echo "Registered patches will be applied the next time 'make' is invoked."
	@echo "To unregister a patch, remove it from ${HV_DIFFCONFIG_LIST}."
  else
	@echo "${PATCH}: No such file or directory"
  endif
else
	@echo "No patch file or directory is specified"
	@echo "Try 'make applydiffconfig PATCH=/path/to/patch' to register patches for generated configuration files."
  ifneq ($(realpath $(HV_DIFFCONFIG_LIST)),)
	@echo "Registers patches:"
	@cat $(HV_DIFFCONFIG_LIST)
  endif
endif

$(HV_DIFFCONFIG_LIST):
	@touch $@

menuconfig:
	@echo "To tweak the configurations, run $(HV_CONFIG_TOOL_DIR)/config_app/app.py using python3 and load $(HV_SCENARIO_XML) in the web browser."

$(HV_CONFIG_DIR):
	@mkdir -p $@

# Legacy variables and targets
ifdef TARGET_DIR
  $(warning TARGET_DIR is obsoleted because generated configuration files are now stored in the build directory)
endif

oldconfig:
	@echo "Generated configuration files are now automatically updated with the scenario XML file."
	@echo "There is no need to invoke oldconfig manually anymore."

update_config:
	@echo "Generated configuration files are now automatically updated with the scenario XML file."
	@echo "There is no need to invoke update_config manually anymore."

CFLAGS += -include $(HV_CONFIG_H)

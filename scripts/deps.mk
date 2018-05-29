# check the existence of a specific executable
# usage: check_dep_exec <executable name>
define check_dep_exec =
BUILD_DEPS += check_$(1)
check_$(1):
	@if ! which $(1) > /dev/null; then   \
	     echo "******** Missing prerequisite tool ********"; \
	     echo "Cannot find executable *$(1)*"; \
	     echo "Please refer to the Getting Started Guide" \
	          "for installation instructions"; \
	     exit 1; \
	 fi
endef

# check the existence of a specific python library
# usage: check_dep_pylib <library name>
define check_dep_pylib =
BUILD_DEPS += check_$(1)
check_$(1):
	@if ! pip list 2>/dev/null | grep $(1) > /dev/null 2>&1; then   \
	     echo "******** Missing prerequisite tool ********"; \
	     echo "The python library *$(1)* is not installed"; \
	     echo "Please refer to the Getting Started Guide" \
	          "for installation instructions"; \
	     exit 1; \
	 fi
endef

# usage: check_dep_exec <executable name> <variable>
#
# Create a target that checks the existence of the specified executable, and
# append that target to the given variable.
define check_dep_exec =
$(2) += check_exec_$(1)
check_exec_$(1):
	@if ! which $(1) > /dev/null; then   \
	     echo "******** Missing prerequisite tool ********"; \
	     echo "Cannot find executable *$(1)*"; \
	     echo "Please refer to the Getting Started Guide" \
	          "for installation instructions"; \
	     exit 1; \
	 fi
endef

# usage: check_dep_py3lib <library name> <variable>
#
# Create a target that checks the existence of the specified python 3 library, and
# append that target to the given variable.
define check_dep_py3lib =
$(2) += check_py3lib_$(1)
check_py3lib_$(1):
	@if ! python3 -c "import $(1)" > /dev/null 2>&1; then   \
	     echo "******** Missing prerequisite tool ********"; \
	     echo "The python3 library *$(1)* is not installed"; \
	     echo "Please refer to the Getting Started Guide" \
	          "for installation instructions"; \
	     exit 1; \
	 fi
endef

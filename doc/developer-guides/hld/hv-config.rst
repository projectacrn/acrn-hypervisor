.. _hv-config:

Compile-Time Configuration
##########################

As described in :ref:`acrn_configuration_tool`, ACRN hypervisor configurations
are saved as XML files and used for compilation. At compile-time, configuration
data in the board and scenario XMLs are converted to C header and source files
that define macros, variables, and data structures to which the hypervisor can
refer. This conversion has two main steps:

1. **Static allocation of resources**, which statically reserves resources for
   the VMs if only high-level requirements are given in the scenario
   configurations. Examples include the runtime base address of the hypervisor
   image and PCI BDF addresses of ivshmem virtual devices.

#. **Generation of C files**, which places the configuration data in the data
   types and structures defined by the hypervisor.

Some key files, which can be found under the build directory of the hypervisor,
are as follows.

- **.board.xml** and **.scenario.xml** These files contain the configuration
  data used by that build.

- **configs/allocation.xml** contains the results of the static allocation.

- **configs/config.mk** This file is a conversion of the hypervisor feature
  configurations (specified in the scenario XML) in Makefile syntax, and can be
  included in makefiles so that the build process can rely on the
  configurations.

- **include/config.h** This file is a conversion of the hypervisor feature
  configurations in C header syntax, and is automatically included in every
  source file so that the values of the configuration symbols are available in
  the sources.

- **configs/boards** and **configs/scenarios** contain all the other generated C
  headers and sources that encode the configuration data in the XML files.

Whenever ``.board.xml`` or ``.scenario.xml`` is modified, the hypervisor will be
rebuilt upon the next invocation of ``make``.

For the concept and usage of the configuration toolset, refer to
:ref:`acrn_configuration_tool`. For a complete list of configuration symbols,
refer to :ref:`scenario-config-options`.

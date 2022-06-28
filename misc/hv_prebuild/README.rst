:orphan:

Hypervisor Pre-Build Check Tool
###############################

There are a number of configuration elements for the ACRN hypervisor that must
defined before building the binaries. Those configuration elements are set
by the user using the :ref:`ACRN Configurator tool <acrn_configuration_tool>`.


This folder holds the source to a tool that is used to ensure that the
configuration is coherent and valid. It is a tool used in the background by
the build system before compiling the hypervisor and other components. It is
not meant to be used as a stand-alone too.

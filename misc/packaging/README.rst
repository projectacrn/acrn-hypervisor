.. _debian_packaging:

ACRN Installation via Debian Packages
#####################################

Debian packages provide a simple way to package ACRN configurations on
a development computer. You can then copy the packages onto your target system,
install the packages, and reboot the system with ACRN and an Ubuntu Service VM
up and running.

ACRN does not distribute pre-built Debian packages for the hypervisor or kernel
because ACRN and the kernel are configured based on your specific hardware and
scenario configurations, as described in the :ref:`overview_dev`. Instead after
configuring ACRN to your needs, Debian packages are created when you build your
ACRN hypervisor and ACRN kernel via ``Makefile`` commands.

All the configuration files and scripts used by the Makefile to build the Debian
packages are in the ``misc/packaging`` folder. The ``gen_acrn_deb.py`` script
does all the work to build the Debian packages so you can copy and install them
on your target system.

For build and installation steps, see :ref:`gsg_build` in the Getting Started
Guide.

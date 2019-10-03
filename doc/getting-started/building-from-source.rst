.. _getting-started-building:

Build ACRN from Source
######################

Introduction
************

Following a general embedded-system programming model, the ACRN
hypervisor is designed to be customized at build time per hardware
platform and per usage scenario, rather than one binary for all
scenarios.

The hypervisor binary is generated based on Kconfig configuration
settings. Instructions about these settings can be found in
:ref:`getting-started-hypervisor-configuration`.

.. note::
   A generic configuration named ``hypervisor/arch/x86/configs/generic.config``
   is provided to help developers try out ACRN more easily.
   This configuration works for most x86-based platforms; it is supported
   with limited features. It can be enabled by specifying ``BOARD=generic``
   in the ``make`` command line.

One binary for all platforms and all usage scenarios is currently not
supported, primarily because dynamic configuration parsing is restricted in
the ACRN hypervisor for the following reasons:

- **Meeting functional safety requirements.** Implementing dynamic parsing
  introduces dynamic objects, which violates functional safety requirements.

- **Reduce complexity.** ACRN is a lightweight reference hypervisor, built for
  embedded IoT. As new platforms for embedded systems are rapidly introduced,
  support for one binary could require more and more complexity in the
  hypervisor, which is something we strive to avoid.

- **Keep small footprint.** Implementing dynamic parsing introduces
  hundreds or thousands of lines of code. Avoiding dynamic parsing
  helps keep the hypervisor's Lines of Code (LOC) in a desirable range (around 30K).

- **Improve boot up time.** Dynamic parsing at runtime increases the boot
  up time. Using a build-time configuration and not dynamic parsing
  helps improve the boot up time of the hypervisor.


Build the ACRN hypervisor, device model, and tools from source by following
these steps.

.. _install-build-tools-dependencies:

Step 1: Install build tools and dependencies
********************************************

ACRN development is supported on popular Linux distributions, each with
their own way to install development tools. This user guide covers the different
steps to configure and build ACRN natively on your distribution. Please refer to
the :ref:`building-acrn-in-docker` user guide for instructions on how to build
ACRN using a container.

  .. note::
     ACRN uses ``menuconfig``, a python3 text-based user interface (TUI) for
     configuring hypervisor options and using python's ``kconfiglib`` library.

Install the necessary tools for the following systems:

* Clear Linux OS development system:

  .. code-block:: none

     $ sudo swupd bundle-add os-clr-on-clr os-core-dev python3-basic
     $ pip3 install --user kconfiglib

* Ubuntu/Debian development system:

  .. code-block:: none

     $ sudo apt install gcc \
          git \
          make \
          gnu-efi \
          libssl-dev \
          libpciaccess-dev \
          uuid-dev \
          libsystemd-dev \
          libevent-dev \
          libxml2-dev \
          libusb-1.0-0-dev \
          python3 \
          python3-pip \
          libblkid-dev \
          e2fslibs-dev \
          pkg-config \
          zlib1g-dev
     $ sudo pip3 install kconfiglib

  .. note::
     Use ``gcc`` version 7.3.* or higher to avoid running into
     issue `#1396 <https://github.com/projectacrn/acrn-hypervisor/issues/1396>`_. Follow these instructions to install the ``gcc-7`` package on Ubuntu 16.04:

     .. code-block:: none

        $ sudo add-apt-repository ppa:ubuntu-toolchain-r/test
        $ sudo apt update
        $ sudo apt install g++-7 -y
        $ sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60 \
                             --slave /usr/bin/g++ g++ /usr/bin/g++-7


     ACRN development requires ``binutils`` version 2.27 (or higher).
     Verify your version of ``binutils`` with the command ``apt show binutils
     ``. While Ubuntu 18.04 has a new version of ``binutils``, the default
     version on Ubuntu 16.04 must be updated (see issue `#1133
     <https://github.com/projectacrn/acrn-hypervisor/issues/1133>`_).

     .. code-block:: none

        $ wget https://mirrors.ocf.berkeley.edu/gnu/binutils/binutils-2.27.tar.gz
        $ tar xzvf binutils-2.27.tar.gz && cd binutils-2.27
        $ ./configure
        $ make
        $ sudo make install


     Ubuntu 14.04 requires ``libsystemd-journal-dev`` instead of ``libsystemd-dev`` as indicated above.

* Fedora/Redhat development system:

  .. code-block:: none

     $ sudo dnf install gcc \
          git \
          make \
          findutils \
          gnu-efi-devel \
          libuuid-devel \
          openssl-devel \
          libpciaccess-devel \
          systemd-devel \
          libxml2-devel \
          libevent-devel \
          libusbx-devel \
          python3 \
          python3-pip \
          libblkid-devel \
          e2fsprogs-devel
     $ sudo pip3 install kconfiglib


* CentOS development system:

  .. code-block:: none

     $ sudo yum install gcc \
             git \
             make \
             gnu-efi-devel \
             libuuid-devel \
             openssl-devel \
             libpciaccess-devel \
             systemd-devel \
             libxml2-devel \
             libevent-devel \
             libusbx-devel \
             python34 \
             python34-pip \
             libblkid-devel \
             e2fsprogs-devel
     $ sudo pip3 install kconfiglib

  .. note::
     You may need to install `EPEL <https://fedoraproject.org/wiki/EPEL>`_
     for installing python3 via yum for CentOS 7. For CentOS 6, you need to
     install pip manually. Refer to https://pip.pypa.io/en/stable/installing
     for details.


Step 2: Get the ACRN hypervisor source code
*******************************************

The `acrn-hypervisor <https://github.com/projectacrn/acrn-hypervisor/>`_
repository contains four main components:

1. The ACRN hypervisor code, located in the ``hypervisor`` directory.
#. The EFI stub code, located in the ``misc/efi-stub`` directory.
#. The ACRN device model code, located in the ``devicemodel`` directory.
#. The ACRN tools source code, located in the ``misc/tools`` directory.

Enter the following to get the acrn-hypervisor source code:

.. code-block:: none

   $ git clone https://github.com/projectacrn/acrn-hypervisor


Step 3: Build with the ACRN scenario
************************************

Currently, the ACRN hypervisor defines these typical usage scenarios:

SDC:
   The SDC (Software Defined Cockpit) scenario defines a simple
   automotive use-case that includes one pre-launched Service VM and one
   post-launched User VM.

SDC2:
   SDC2 (Software Defined Cockpit 2) is an extended scenario for an
   automotive SDC system.  SDC2 defines one pre-launched Service VM and up
   to three post-launched VMs.

LOGICAL_PARTITION:
    This scenario defines two pre-launched VMs.

INDUSTRY:
   This is a typical scenario for industrial usage with up to four VMs:
   one pre-launched Service VM, one post-launched Standard VM for Human
   interaction (HMI), and one or two post-launched RT VMs for real-time
   control.

HYBRID:
   This scenario defines a hybrid use case with three VMs: one
   pre-launched VM, one pre-launched Service VM, and one post-launched
   Standard VM.

Assuming that you are at the top level of the acrn-hypervisor directory:

* Build ``INDUSTRY`` scenario on ``nuc7i7dnb``:

  .. code-block:: none

     $ make all BOARD=nuc7i7dnb SCENARIO=industry

* Build ``SDC`` scenario on ``nuc6cayh``:

  .. code-block:: none

     $ make all BOARD=nuc6cayh SCENARIO=sdc

See the :ref:`hardware` document for information about the platform needs
for each scenario.

.. _getting-started-hypervisor-configuration:

Step 4: Build the hypervisor configuration
******************************************

Modify the hypervisor configuration
===================================

The ACRN hypervisor leverages Kconfig to manage configurations; it is
powered by ``Kconfiglib``. A default configuration is generated based on the
board you have selected via the ``BOARD=`` command line parameter. You can
make further changes to that default configuration to adjust to your specific
requirements.

To generate hypervisor configurations, you must build the hypervisor
individually. The following steps generate a default but complete
configuration, based on the platform selected, assuming that you are at the
top level of the acrn-hypervisor directory. The configuration file, named
``.config``, can be found under the target folder of your build.

.. code-block:: none

   $ cd hypervisor
   $ make defconfig BOARD=nuc6cayh

The BOARD specified is used to select a ``defconfig`` under
``arch/x86/configs/``. The other command line-based options (e.g. ``RELEASE``)
take no effect when generating a defconfig.

To modify the hypervisor configurations, you can either edit ``.config``
manually, or invoke a TUI-based menuconfig, powered by kconfiglib, by
executing ``make menuconfig``. As an example, the following commands
(assuming that you are at the top level of the acrn-hypervisor directory)
generate a default configuration file for UEFI, allowing you to modify some
configurations and build the hypervisor using the updated ``.config``:

.. code-block:: none

   # Modify the configurations per your needs
   $ cd ../         # Enter top-level folder of acrn-hypervisor source
   $ make menuconfig -C hypervisor BOARD=kbl-nuc-i7   <select industry scenario>


Note that ``menuconfig`` is python3 only.

Refer to the help on menuconfig for a detailed guide on the interface:

.. code-block:: none

   $ pydoc3 menuconfig

Step 5: Build the hypervisor, device model, and tools
*****************************************************

Now you can build all these components at once as follows:

.. code-block:: none

   $ make FIRMWARE=uefi          # Build the UEFI hypervisor with the new .config

The build results are found in the ``build`` directory. You can specify
a different Output folder by setting the ``O`` ``make`` parameter,
for example: ``make O=build-nuc BOARD=nuc6cayh``.

If you only need the hypervisor, use this command:

.. code-block:: none

   $ make clean                       # Remove files previously built
   $ make -C hypervisor
   $ make -C misc/efi-stub HV_OBJDIR=$PWD/hypervisor/build EFI_OBJDIR=$PWD/hypervisor/build

The ``acrn.efi`` will be generated in the ``./hypervisor/build/acrn.efi`` directory hypervisor.

As mentioned in :ref:`ACRN Configuration Tool <vm_config_workflow>`, the Board configuration and VM configuration can be imported from XML files.
If you want to build the hypervisor with XML configuration files, specify
the file location as follows:

.. code-block:: none

   $ make BOARD_FILE=$PWD/misc/acrn-config/xmls/board-xmls/nuc7i7dnb.xml \
   SCENARIO_FILE=$PWD/misc/acrn-config/xmls/config-xmls/nuc7i7dnb/industry.xml FIRMWARE=uefi


Note that the file path must be absolute. Both of the ``BOARD`` and ``SCENARIO`` parameters are not needed because the information is retrieved from the XML file. Adjust the example above to your own environment path.

Follow the same instructions to boot and test the images you created from your build.


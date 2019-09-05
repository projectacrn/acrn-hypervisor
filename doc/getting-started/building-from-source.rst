.. _getting-started-building:

Build ACRN from Source
######################

Introduction
************

Following a general embedded system programming model, the ACRN
hypervisor is designed to be customized at build-time per hardware
platform and per usage scenario, rather than one binary for all
scenarios.

The hypervisor binary is generated based on Kconfig configuration
settings.  Instruction about these settings can be found in
:ref:`getting-started-hypervisor-configuration`.

.. note::
   A generic configuration named ``hypervisor/arch/x86/configs/generic.config``
   is provided to help developers try out ACRN more easily. This configuration
   will likely work for most x86-based platforms, supported with limited features.
   This configuration can be enabled by specifying ``BOARD=generic`` in
   the make command line.


A primary reason one binary for all platforms and all usage scenarios is
not supported is because dynamic configuration parsing is restricted in
ACRN hypervisor, for the following considerations:

* **Meeting functional safety requirements** Absence of dynamic objects is
  required in functional safety standards. Implementation of dynamic parsing
  would introduce dynamic objects. Avoiding use of dynamic
  parsing would help the ACRN hypervisor meet functional safety requirements.

* **Reduce complexity** ACRN is a lightweight reference hypervisor, built for
  embedded IoT. As new platforms for embedded systems are rapidly introduced,
  support for one binary would require more and more complexity in the
  hypervisor, something we need to avoid.

* **Keep small footprint** Implementation of dynamic parsing would introduce
  hundreds or thousands of code. Avoiding dynamic parsing would help keep
  Lines of Code (LOC) of the hypervisor in a desirable range (around 30K).

* **Improve boot up time** Dynamic parsing at runtime would increase the boot
  up time. Using build-time configuration and not dynamic parsing would help
  improve boot up time of the hypervisor.


You can build the ACRN hypervisor, device model, and tools from
source, by following these steps.

Install build tools and dependencies
************************************

ACRN development is supported on popular Linux distributions,
each with their own way to install development tools:

  .. note::
     ACRN uses ``menuconfig``, a python3 text-based user interface (TUI) for
     configuring hypervisor options and using python's ``kconfiglib`` library.

* On a Clear Linux OS development system, install the necessary tools:

  .. code-block:: none

     $ sudo swupd bundle-add os-clr-on-clr os-core-dev python3-basic
     $ pip3 install --user kconfiglib

* On a Ubuntu/Debian development system:

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
     You need to use ``gcc`` version 7.3.* or higher else you will run into issue
     `#1396 <https://github.com/projectacrn/acrn-hypervisor/issues/1396>`_. Follow
     these instructions to install the ``gcc-7`` package on Ubuntu 16.04:

     .. code-block:: none

        $ sudo add-apt-repository ppa:ubuntu-toolchain-r/test
        $ sudo apt update
        $ sudo apt install g++-7 -y
        $ sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60 \
                             --slave /usr/bin/g++ g++ /usr/bin/g++-7

  .. note::
     ACRN development requires ``binutils`` version 2.27 (or higher). You can
     verify your version of ``binutils`` with the command ``apt show binutils``.
     While Ubuntu 18.04 has a new version of ``binutils`` the default version on
     Ubuntu 16.04 needs updating (see issue `#1133
     <https://github.com/projectacrn/acrn-hypervisor/issues/1133>`_).

     .. code-block:: none

        $ wget https://mirrors.ocf.berkeley.edu/gnu/binutils/binutils-2.27.tar.gz
        $ tar xzvf binutils-2.27.tar.gz && cd binutils-2.27
        $ ./configure
        $ make
        $ sudo make install

  .. note::
     Ubuntu 14.04 requires ``libsystemd-journal-dev`` instead of ``libsystemd-dev``
     as indicated above.

* On a Fedora/Redhat development system:

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


* On a CentOS development system:

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
     You may need to install `EPEL <https://fedoraproject.org/wiki/EPEL>`_ for
     installing python3 via yum for CentOS 7. For CentOS 6 you need to install
     pip manually. Please refer to https://pip.pypa.io/en/stable/installing for
     details.


Get the ACRN hypervisor source code
***********************************

The `acrn-hypervisor <https://github.com/projectacrn/acrn-hypervisor/>`_
repository has four main components in it:

1. The ACRN hypervisor code located in the ``hypervisor`` directory
#. The EFI stub code located in the ``misc/efi-stub`` directory
#. The ACRN devicemodel code located in the ``devicemodel`` directory
#. The ACRN tools source code located in the ``misc/tools`` directory

Follow this step to get the acrn-hypervisor source code:

.. code-block:: none

   $ git clone https://github.com/projectacrn/acrn-hypervisor


Choose the ACRN scenario
************************

.. note:: Documentation about the new ACRN use-case scenarios is a
   work-in-progress on the master branch as we work towards the v1.2
   release.

Currently ACRN hypervisor defines these typical usage scenarios:

SDC:
   The SDC (Software Defined Cockpit) scenario defines a simple
   automotive use-case where there is one pre-launched Service VM and one
   post-launched User VM.

SDC2:
   SDC2 (Software Defined Cockpit 2) is an extended scenario for an
   automotive SDC system.  SDC2 defined one pre-launched Service VM and up
   to three post-launched VMs.

LOGICAL_PARTITION:
    This scenario defines two pre-launched VMs.

INDUSTRY:
   This is a typical scenario for industrial usage with up to four VMs:
   one pre-launched Service VM, one post-launched Standard VM for Human
   interaction (HMI), and one or two post-launched RT VMs for real-time
   control.

HYBRID:
   This scenario defines a hybrid use-case with three VMs: one
   pre-launched VM, one pre-launched Service VM, and one post-launched
   Standard VM.

You can select a build scenario by changing the default Kconfig name in
the choice block of **ACRN Scenario** in ``arch/x86/Kconfig``. The
corresponding VM configuration files in the corresponding
``scenarios/$SCENARIO_NAME/`` folder.

.. code-block:: none
   :emphasize-lines: 7

   $ cd  acrn-hypervisor/hypervisor
   $ sudo vim arch/x86/Kconfig
   # <Fill the scenario name into below and save>

   choice
                prompt "ACRN Scenario"
                default SDC

See the :ref:`hardware` document for information about the platform
needs for each scenario.

.. _getting-started-hypervisor-configuration:

Modify the hypervisor configuration
***********************************

The ACRN hypervisor leverages Kconfig to manage configurations, powered by
Kconfiglib. A default configuration is generated based on the board you have
selected via the ``BOARD=`` command line parameter. You can make further
changes to that default configuration to adjust to your specific
requirements.

To generate hypervisor configurations, you need to build the hypervisor
individually. The following steps generate a default but complete configuration,
based on the platform selected, assuming that you are under the top-level
directory of acrn-hypervisor. The configuration file, named ``.config``, can be
found under the target folder of your build.

.. code-block:: none

   $ make defconfig BOARD=nuc6cayh

The BOARD specified is used to select a defconfig under
``arch/x86/configs/``. The other command-line based options (e.g. ``RELEASE``)
take no effects when generating a defconfig.

To modify the hypervisor configurations, you can either edit ``.config``
manually, or invoke a TUI-based menuconfig, powered by kconfiglib, by executing
``make menuconfig``. As an example, the following commands, assuming that you
are under the top-level directory of acrn-hypervisor, generate a default
configuration file for UEFI, allow you to modify some configurations and build
the hypervisor using the updated ``.config``.

.. code-block:: none

   $ make menuconfig              # Modify the configurations per your needs

.. note::
   Menuconfig is python3 only.

Refer to the help on menuconfig for a detailed guide on the interface.

.. code-block:: none

   $ pydoc3 menuconfig

Build the hypervisor, device model and tools
********************************************

Now you can build all these components in one go as follows:

.. code-block:: none

   $ cd ../                      # Enter top-level folder of acrn-hypervisor source
   $ make FIRMWARE=uefi          # Build the UEFI hypervisor with the new .config

The build results are found in the ``build`` directory.  You can specify
use a different Output folder by setting the ``O`` make parameter,
for example: ``make O=build-nuc BOARD=nuc6cayh``.

If you only need the hypervisor, then use this command:

.. code-block:: none

   $ make clean                              # Remove files previously built
   $ make FIRMWARE=uefi hypervisor           # This will only build the hypervisor

You could also use ``FIRMWARE=sbl`` instead, to build the Intel SBL
(`Slim bootloader
<https://www.intel.com/content/www/us/en/design/products-and-solutions/technologies/slim-bootloader/overview.html>`_)
hypervisor.

Follow the same instructions to boot and test the images you created from your build.

Save as default configuration
*****************************

Currently the ACRN hypervisor looks for default configurations under
``hypervisor/arch/x86/configs/<BOARD>.config``, where ``<BOARD>`` is the
specified platform. The following steps allow you to create a defconfig for
another platform based on a current one.

   .. code-block:: none

      $ cd hypervisor
      $ make defconfig BOARD=nuc6cayh
      $ make menuconfig         # Modify the configurations
      $ make savedefconfig      # The minimized config reside at build/defconfig
      $ cp build/defconfig arch/x86/configs/xxx.config

Then you can re-use that configuration by passing the name (``xxx`` in the
example above) to 'BOARD=':

   .. code-block:: none

      $ make defconfig BOARD=xxx

.. _getting-started-building:

Build ACRN from Source
######################

If you would like to build the ACRN hypervisor, device model, and tools from
source, follow these steps.

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
          pkg-config
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


Build the hypervisor, device model and tools
********************************************

The `acrn-hypervisor <https://github.com/projectacrn/acrn-hypervisor/>`_
repository has four main components in it:

1. The ACRN hypervisor code located in the ``hypervisor`` directory
#. The EFI stub code located in the ``efi-stub`` directory
#. The ACRN devicemodel code located in the ``devicemodel`` directory
#. The ACRN tools source code located in the ``tools`` directory

You can build all these components in one go as follows:

.. code-block:: none

   $ git clone https://github.com/projectacrn/acrn-hypervisor
   $ cd acrn-hypervisor
   $ make

The build results are found in the ``build`` directory.

.. note::
   if you wish to use a different target folder for the build
   artefacts, set the ``O`` (that is capital letter 'O') to the
   desired value. Example: ``make O=build-nuc BOARD=nuc6cayh``.

Generating the documentation is described in details in the :ref:`acrn_doc`
tutorial.

Follow the same instructions to boot and test the images you created
from your build.

.. _getting-started-hypervisor-configuration:

Configuring the hypervisor
**************************

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

      $ cd hypervisor
      $ make defconfig BOARD=nuc6cayh

The BOARD specified is used to select a defconfig under
``arch/x86/configs/``. The other command-line based options (e.g. ``RELEASE``)
take no effects when generating a defconfig.

Modify the hypervisor configurations
************************************

To modify the hypervisor configurations, you can either edit ``.config``
manually, or invoke a TUI-based menuconfig, powered by kconfiglib, by executing
``make menuconfig``. As an example, the following commands, assuming that you
are under the top-level directory of acrn-hypervisor, generate a default
configuration file for UEFI, allow you to modify some configurations and build
the hypervisor using the updated ``.config``.

   .. code-block:: none

      $ cd hypervisor
      $ make defconfig BOARD=nuc6cayh
      $ make menuconfig              # Modify the configurations per your needs
      $ make                         # Build the hypervisor with the new .config

   .. note::
      Menuconfig is python3 only.

Refer to the help on menuconfig for a detailed guide on the interface.

   .. code-block:: none

      $ pydoc3 menuconfig

Create a new default configuration
**********************************

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


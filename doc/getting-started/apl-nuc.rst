.. _getting-started-apl-nuc:

Getting started guide for Intel NUC
###################################

The Intel |reg| NUC (NUC6CAYH) is the primary tested
platform for ACRN development, and its setup is described below.


Hardware setup
**************

Two Apollo Lake Intel platforms, described in :ref:`hardware`, are currently
supported for ACRN development:

- The `UP Squared board <http://www.up-board.org/upsquared/>`_ (UP2) is also
  known to work and its setup is described in :ref:`getting-started-up2`.

Firmware update on the NUC
==========================

You may need to update to the latest UEFI firmware for the NUC hardware.
Follow these `BIOS Update Instructions
<https://www.intel.com/content/www/us/en/support/articles/000005636.html>`__
for downloading and flashing an updated BIOS for the NUC.

Software setup
**************

Set up a Clear Linux Operating System
=====================================

Currently, an installable version of ARCN does not exist. Therefore, you
need to setup a base Clear Linux OS and you'll build and bootstrap ACRN
on your platform. You'll need a network connection for your platform to
complete this setup.

.. note::
   ACRN v.01 or latest requires Clear Linux version 23690 or newer. The instructions below
   have been validated with version 23690 and need some adjustment to work
   with newer versions. You will see a note when the instruction needs to be
   adjusted.

#. Download the compressed Clear installer image from
   https://download.clearlinux.org/releases/23690/clear/clear-23690-installer.img.xz
   and follow the `Clear Linux installation guide
   <https://clearlinux.org/documentation/clear-linux/get-started/bare-metal-install>`__
   as a starting point for installing Clear Linux onto your platform.  Follow the recommended
   options for choosing an **Automatic** installation type, and using the platform's
   storage as the target device for installation (overwriting the existing data
   and creating three partitions on the platform's storage drive).

#. After installation is complete, boot into Clear Linux, login as
   **root**, and set a password.

#. Clear Linux is set to automatically update itself. We recommend that you disable
   this feature to have more control over when the updates happen. Use this command
   to disable the autoupdate feature:

   .. code-block:: none

      # swupd autoupdate --disable

#. if an old Clear Linux version installed already on your Hardware. We recommend that you upgrade Clear Linux to 23690 
   or newer to use ACRN v0.1 or latest, Use this command
   to upgrade your Clear Linux version to 23690 or <latest version>:

   .. code-block:: none

      # swupd update -m 23690 or <latest version>


#. Use the ``swupd bundle-add`` command and add these Clear Linux bundles:

   .. code-block:: none

      # swupd bundle-add vim network-basic service-os kernel-pk desktop openssh-server

   .. table:: Clear Linux bundles
      :widths: auto
      :name: CL-bundles

      +--------------------+---------------------------------------------------+
      | Bundle             | Description                                       |
      +====================+===================================================+
      | vim                | vim text editor                                   |
      +--------------------+---------------------------------------------------+
      | network-basic      | Run network utilities and modify network settings |
      +--------------------+---------------------------------------------------+
      | service-os         | Add the acrn hypervisor, the acrn devicemodel and |
      |                    | Service OS kernel                                 |
      +--------------------+---------------------------------------------------+
      | kernel-pk          | Run the Intel "PK" kernel(product kernel source)  |
      |                    | and enterprise-style kernel with backports        |
      +--------------------+---------------------------------------------------+
      | desktop            | desktop application, Weston included              |
      +--------------------+---------------------------------------------------+
      | openssh-server     | include openssh                                   |
      +--------------------+---------------------------------------------------+

Add the ACRN hypervisor to the EFI Partition
============================================

In order to boot the ACRN SOS on the platform, you'll need to add it to the EFI
partition. Follow these steps:

#. Mount the EFI partition and verify you have the following files:

   .. code-block:: none

      # mount /dev/sda1 /mnt

      # ls -1 /mnt/EFI/org.clearlinux
      bootloaderx64.efi
      kernel-org.clearlinux.native.4.17.6-590
      kernel-org.clearlinux.pk414-sos.4.14.52-63
      kernel-org.clearlinux.pk414-standard.4.14.52-63
      loaderx64.efi

   .. note::
      The Clear Linux project releases updates often, sometimes
      twice a day, so make note of the specific kernel versions (``*-sos``
      and ``*-standard``) listed on your system,
      as you will need them later.

   .. note::
      The EFI System Partition (ESP) may be different based on your hardware.
      It will typically be something like ``/dev/mmcblk0p1`` on platforms
      that have an on-board eMMC or ``/dev/nvme0n1p1`` if your system has
      a non-volatile storage media attached via a PCI Express (PCIe) bus
      (NVMe).

#. Put the ``acrn.efi`` hypervisor application (included in the Clear
   Linux release) on the EFI partition with:

   .. code-block:: none

      # mkdir /mnt/EFI/acrn
      # cp /usr/lib/acrn/acrn.efi /mnt/EFI/acrn/

#. Configure the EFI firmware to boot the ACRN hypervisor by default

   The ACRN hypervisor (``acrn.efi``) is an EFI executable
   loaded directly by the platform EFI firmware. It then in turns loads the
   Service OS bootloader. Use the ``efibootmgr`` utility to configure the EFI
   firmware and add a new entry that loads the ACRN hypervisor.

   .. code-block:: none

      # efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L ACRN

   .. note::

      Be aware that a Clearlinux update that includes a kernel upgrade will
      reset the boot option changes you just made. A Clearlinux update could
      happen automatically (if you have not disabled it as described above),
      if you later install a new bundle to your system, or simply if you
      decide to trigger an update manually. Whenever that happens,
      double-check the platform boot order using ``efibootmgr -v`` and
      modify it if needed.

   The ACRN hypervisor (``acrn.efi``) accepts two command-line parameters that
   tweak its behaviour:

   1. ``bootloader=``: this sets the EFI executable to be loaded once the hypervisor
      is up and running. This is typically the bootloader of the Service OS and the
      default value is to use the Clearlinux bootloader, i.e.:
      ``\EFI\org.clearlinux\bootloaderx64.efi``.
   #. ``uart=``: this tells the hypervisor where the serial port (UART) is found or
      whether it should be disabled. There are three forms for this parameter:

      #. ``uart=disabled``: this disables the serial port completely
      #. ``uart=mmio@<MMIO address>``: this sets the serial port MMIO address
      #. ``uart=port@<port address>``: this sets the serial port address

   Here is a more complete example of how to configure the EFI firmware to load the ACRN
   hypervisor and set these parameters.

   .. code-block:: none

      # efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L "ACRN Hypervisor" \
            -u "bootloader=\EFI\org.clearlinux\bootloaderx64.efi uart=mmio@0x9141e000"

#. Create a boot entry for the ACRN Service OS by copying a provided ``acrn.conf``
   and editing it to account for the kernel versions noted in a previous step.

   It must contain these settings:

   +-----------+----------------------------------------------------------------+
   | Setting   | Description                                                    |
   +===========+================================================================+
   | title     | Text to show in the boot menu                                  |
   +-----------+----------------------------------------------------------------+
   | linux     | Linux kernel for the Service OS (\*-sos)                       |
   +-----------+----------------------------------------------------------------+
   | options   | Options to pass to the Service OS kernel (kernel parameters)   |
   +-----------+----------------------------------------------------------------+

   A starter acrn.conf configuration file is included in the Clear Linux release and is
   also available in the acrn-hypervisor/hypervisor GitHub repo as `acrn.conf
   <https://github.com/projectacrn/acrn-hypervisor/hypervisor/tree/master/bsp/uefi/clearlinux/acrn.conf>`__
   as shown here:

   .. literalinclude:: ../../hypervisor/bsp/uefi/clearlinux/acrn.conf
      :caption: hypervisor/bsp/uefi/clearlinux/acrn.conf

   On the platform, copy the ``acrn.conf`` file to the EFI partition we mounted earlier:

   .. code-block:: none

      # cp /usr/share/acrn/samples/nuc/acrn.conf /mnt/loader/entries/

   You will need to edit this file to adjust the kernel version (``linux`` section)
   and also insert the ``PARTUUID`` of your ``/dev/sda3`` partition
   (``root=PARTUUID=<><UUID of rootfs partition>``) in the ``options`` section.

   Use ``blkid`` to find out what your ``/dev/sda3`` ``PARTUUID`` value is.

   .. note::
      It is also possible to use the device name directly, e.g. ``root=/dev/sda3``

#. Add a timeout period for Systemd-Boot to wait, otherwise it will not
   present the boot menu and will always boot the base Clear Linux

   .. code-block:: none

      # clr-boot-manager set-timeout 20
      # clr-boot-manager update

#. Reboot and select "The ACRN Service OS" to boot, as shown in
   :numref:`gsg-bootmenu`:

   .. figure:: images/gsg-bootmenu.png
      :align: center
      :width: 650px
      :name: gsg-bootmenu

      ACRN Service OS Boot menu

#. After booting up the ACRN hypervisor, the Service OS will be launched
   automatically by default, as shown in :numref:`gsg-sos-console`:

   .. figure:: images/gsg-sos-console.png
      :align: center
      :name: gsg-sos-console

      Service OS Console

   ..  note:: You may need to hit ``Enter`` to get a clean login prompt

#. From here you can login as root using the password you set previously when
   you installed Clear Linux.

ACRN Network Bridge
===================

ACRN bridge has been setup as a part of systemd services for device communication. The default
bridge creates ``acrn_br0`` which is the bridge and ``acrn_tap0`` as an initial setup. The files can be
found in ``/usr/lib/systemd/network``. No additional setup is needed since systemd-networkd is
automatically enabled after a system restart.

Set up Reference UOS
====================

#. On your platform, download the pre-built reference Clear Linux UOS image 23690 or newer into your
   (root) home directory:

   .. code-block:: none

      # cd ~
      # curl -O https://download.clearlinux.org/releases/23690/clear/clear-23690-kvm.img.xz

   .. note::
      In case you want to use or try out a newer version of Clear Linux as the UOS, you can
      download the latest from http://download.clearlinux.org/image. Make sure to adjust the steps
      described below accordingly (image file name and kernel modules version).

#. Uncompress it:

   .. code-block:: none

      # unxz clear-23690-kvm.img.xz

#. Deploy the UOS kernel modules to UOS virtual disk image (note: you'll need to use
   the same **standard** image version number noted in step 1 above):

   .. code-block:: none

      # losetup -f -P --show /root/clear-23690-kvm.img
      # mount /dev/loop0p3 /mnt
      # cp -r /usr/lib/modules/4.14.52-63.pk414-standard /mnt/lib/modules/
      # umount /mnt
      # sync

#. Edit and Run the ``launch_uos.sh`` script to launch the UOS.

   A sample `launch_uos.sh
   <https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/devicemodel/samples/nuc/launch_uos.sh>`__
   is included in the Clear Linux release, and
   is also available in the acrn-hypervisor/devicemodel GitHub repo (in the samples
   folder) as shown here:

   .. literalinclude:: ../../devicemodel/samples/nuc/launch_uos.sh
      :caption: devicemodel/samples/nuc/launch_uos.sh
      :language: bash
      :emphasize-lines: 26,28

   .. note::
      In case you have downloaded a different Clear Linux image than the one above
      (``clear-22140-kvm.img.xz``), you will need to modify the Clear Linux file name
      and version number highlighted above (the ``-s 3,virtio-blk`` argument) to match
      what you have downloaded above. Likewise, you may need to adjust the kernel file
      name on the second line highlighted (check the exact name to be used using:
      ``ls /usr/lib/kernel/org.clearlinux*-standard*``).

   By default, the script is located in the ``/usr/share/acrn/samples/nuc/``
   directory. You can edit it there, and then run it to launch the User OS:

   .. code-block:: none

      # cd /usr/share/acrn/samples/nuc/
      # ./launch_uos.sh

#. At this point, you've successfully booted the ACRN hypervisor,
   SOS, and UOS:

   .. figure:: images/gsg-successful-boot.png
      :align: center
      :name: gsg-successful-boot


Device Manager memory allocation mechanism
==========================================

The ACRN Device Manager (DM) virtual memory allocation uses the HugeTLB mechanism.
(You can read more about `HugeTLB in the linux kernel <https://linuxgazette.net/155/krishnakumar.html>`_
for more information about how this mechanism works.)

For hugeTLB to work, you'll need to reserve huge pages:

  - For a (large) 1GB huge page reservation, add ``hugepagesz=1G hugepages=reserved_pg_num``
    (for example, ``hugepagesz=1G hugepages=4``) to the SOS cmdline in
    ``acrn.conf`` (for EFI)

  - For a (smaller) 2MB huge page reservation, after the SOS starts up, run the
    command::

       echo reserved_pg_num > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

  .. note::
     You can use 2M reserving method to do reservation for 1G page size, but it
     may fail.  For an EFI platform, you may skip 1G page reservation
     by using a 2M page, but make sure your huge page reservation size is
     large enough for your usage.

Build ACRN from Source
**********************

If you would like to build ACRN hypervisor and device model from source,
follow these steps.

Install build tools and dependencies
====================================

ARCN development is supported on popular Linux distributions,
each with their own way to install development tools:

  .. note::
     ACRN uses ``menuconfig``, a python3 text-based user interface (TUI) for
     configuring hypervisor options and using python's ``kconfiglib`` library.

* On a Clear Linux development system, install the ``os-clr-on-clr`` bundle to get
  the necessary tools:

  .. code-block:: console

     $ sudo swupd bundle-add os-clr-on-clr
     $ sudo swupd bundle-add python3-basic
     $ sudo pip3 install kconfiglib

* On a Ubuntu/Debian development system:

  .. code-block:: console

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
          python3-pip
     $ sudo pip3 install kconfiglib

  .. note::
     Ubuntu 14.04 requires ``libsystemd-journal-dev`` instead of ``libsystemd-dev``
     as indicated above.

* On a Fedora/Redhat development system:

  .. code-block:: console

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
          python3-pip
     $ sudo pip3 install kconfiglib


* On a CentOS development system:

  .. code-block:: console

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
             python34-pip
     $ sudo pip3 install kconfiglib

  .. note::
     You may need to install `EPEL <https://fedoraproject.org/wiki/EPEL>`_ for
     installing python3 via yum for CentOS 7. For CentOS 6 you need to install
     pip manually. Please refer to https://pip.pypa.io/en/stable/installing for
     details.


Build the hypervisor, device model and tools
============================================

The `acrn-hypervisor <https://github.com/projectacrn/acrn-hypervisor/>`_
repository has three main components in it:

1. The ACRN hypervisor code located in the ``hypervisor`` directory
#. The ACRN devicemodel code located in the ``devicemodel`` directory
#. The ACRN tools source code located in the ``tools`` directory

You can build all these components in one go as follows:

.. code-block:: console

   $ git clone https://github.com/projectacrn/acrn-hypervisor
   $ cd acrn-hypervisor
   $ make

The build results are found in the ``build`` directory.

.. note::
   if you wish to use a different target folder for the build
   artefacts, set the ``O`` (that is capital letter 'O') to the
   desired value. Example: ``make O=build-uefi PLATFORM=uefi``.

You can also build these components individually. The following
steps assume that you have already cloned the ``acrn-hypervisor`` repository
and are using it as the current working directory.

#. Build the ACRN hypervisor.

   .. code-block:: console

      $ cd hypervisor
      $ make PLATFORM=uefi

   The build results are found in the ``build`` directory.

#. Build the ACRN device model (included in the acrn-hypervisor repo):

   .. code-block:: console

      $ cd ../devicemodel
      $ make

   The build results are found in the ``build`` directory.

#. Build the ACRN tools (included in the acrn-hypervisor repo):

   .. code-block:: console

      $ cd ../tools
      $ for d in */; do make -C "$d"; done

Follow the same instructions to boot and test the images you created
from your build.

Generate the hypervisor configurations
======================================

The ACRN hypervisor leverages Kconfig to manage configurations, powered by
Kconfiglib. A default configuration is generated based on the platform you have
selected via the ``PLATFORM=`` command line parameter. You can make further
changes to that default configuration to adjust to your specific
requirements.

To generate hypervisor configurations, you need to build the hypervisor
individually. The following steps generate a default but complete configuration,
based on the platform selected, assuming that you are under the top-level
directory of acrn-hypervisor. The configuration file, named ``.config``, can be
found under the target folder of your build.

   .. code-block:: console

      $ cd hypervisor
      $ make defconfig PLATFORM=uefi

The PLATFORM specified is used to select a defconfig under
``arch/x86/configs/``. The other command-line based options (e.g. ``RELEASE``)
take no effects when generating a defconfig.

Modify the hypervisor configurations
====================================

To modify the hypervisor configurations, you can either edit ``.config``
manually, or invoke a TUI-based menuconfig, powered by kconfiglib, by executing
``make menuconfig``. As an example, the following commands, assuming that you
are under the top-level directory of acrn-hypervisor, generate a default
configuration file for UEFI, allow you to modify some configurations and build
the hypervisor using the updated ``.config``.

   .. code-block:: console

      $ cd hypervisor
      $ make defconfig PLATFORM=uefi
      $ make menuconfig              # Modify the configurations per your needs
      $ make                         # Build the hypervisor with the new .config

   .. note::
      Menuconfig is python3 only.

Refer to the help on menuconfig for a detailed guide on the interface.

   .. code-block:: console

      $ pydoc3 menuconfig

Create a new default configuration
==================================

Currently the ACRN hypervisor looks for default configurations under
``hypervisor/arch/x86/configs/<PLATFORM>.config``, where ``<PLATFORM>`` is the
specified platform. The following steps allow you to create a defconfig for
another platform based on a current one.

   .. code-block:: console

      $ cd hypervisor
      $ make defconfig PLATFORM=uefi
      $ make menuconfig         # Modify the configurations
      $ make savedefconfig      # The minimized config reside at build/defconfig
      $ cp build/defconfig arch/x86/configs/xxx.config

Then you can re-use that configuration by passing the name (``xxx`` in the
example above) to 'PLATFORM=':

   .. code-block:: console

      $ make defconfig PLATFORM=xxx

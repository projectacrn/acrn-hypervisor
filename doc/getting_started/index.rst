.. _getting_started:

Getting Started Guide
#####################

After reading the :ref:`introduction`, use this guide to get started
using ACRN in a reference setup.  We'll show how to set up your
development and target hardware, and then how to boot up the ACRN
hypervisor and the `Clear Linux`_ Service OS and Guest OS on the Intel
|reg| NUC.

.. _Clear Linux: https://clearlinux.org

Hardware setup
**************

The Intel |reg| NUC (NUC6CAYH) is the supported reference target
platform for ACRN work, as described in :ref:`hardware`, and is the only
platform currently tested with these setup instructions.

The recommended NUC hardware configuration is:

- NUC: `NUC Kit
  NUC6CAYH <https://www.intel.com/content/www/us/en/products/boards-kits/nuc/kits/nuc6cayh.html>`__
- `UEFI BIOS (version 0047) <https://downloadcenter.intel.com/product/95062/Intel-NUC-Kit-NUC6CAYH>`__.
- Memory: 8G DDR3
- SSD: 120G SATA

Software setup
**************

Firmware update on the NUC
==========================

You may need to update to the latest UEFI firmware for the NUC hardware.
Follow these `BIOS Update Instructions
<https://www.intel.com/content/www/us/en/support/articles/000005636.html>`__
for downloading and flashing an updated BIOS for the NUC.

Set up a Clear Linux Operating System
=====================================

Currently, an installable version of ARCN does not exist. Therefore, you
need to setup a base Clear Linux OS to bootstrap ACRN on the NUC. You'll
need a network connection for your NUC to complete this setup.

.. note::
   ACRN requires Clear Linux version 22140 or newer. The instructions below
   have been validated with version 22140 and need some adjustment to work
   with newer versions. You will see a note when the instruction needs to be
   adjusted.

#. Download the compressed Clear installer image from
   https://download.clearlinux.org/releases/22140/clear/clear-22140-installer.img.xz
   and follow the `Clear Linux installation guide
   <https://clearlinux.org/documentation/clear-linux/get-started/bare-metal-install>`__
   as a starting point for installing Clear Linux onto your NUC.  Follow the recommended
   options for choosing an **Automatic** installation type, and using the NUC's
   storage as the target device for installation (overwriting the existing data
   and creating three partitions on the NUC's SSD drive).

#. After installation is complete, boot into Clear Linux, login as
   **root**, and set a password.

#. Clear Linux is set to automatically update itself. We recommend that you disable
   this feature to have more control over when the updates happen. Use this command
   to disable the autoupdate feature:

   .. code-block:: none

      # swupd autoupdate --disable

#. Use the ``swupd bundle-add`` command and add these Clear Linux bundles:

   .. code-block:: none

      # swupd bundle-add vim network-basic service-os kernel-pk

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

Add the ACRN hypervisor to the EFI Partition
============================================

In order to boot the ACRN SOS on the NUC, you'll need to add it to the EFI
partition. Follow these steps:

#. Mount the EFI partition and verify you have the following files:

   .. code-block:: none

      # mount /dev/sda1 /mnt

      # ls -1 /mnt/EFI/org.clearlinux
      bootloaderx64.efi
      kernel-org.clearlinux.native.4.16.6-563
      kernel-org.clearlinux.pk414-sos.4.14.34-28
      kernel-org.clearlinux.pk414-standard.4.14.34-28
      loaderx64.efi

   .. note::
      The Clear Linux project releases updates often, sometimes
      twice a day, so make note of the specific kernel versions (``*-sos``
      and ``*-standard``) listed on your system,
      as you will need them later.

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

      # efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda1 -p 1 -L ACRN
      # cd /mnt/EFI/org.clearlinux/
      # cp bootloaderx64.efi bootloaderx64_origin.efi

   .. note::
      Be aware that a Clearlinux update that includes a kernel upgrade will
      reset the boot option changes you just made.. A Clearlinux update could
      happen automatically (if you have
      not disabled it as described above), if you later install a new
      bundle to your system, or simply if you decide to trigger an update
      manually. Whenever that happens, double-check the platform boot order
      using ``efibootmgr -v`` and modify it if needed.

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

   On the NUC, copy the ``acrn.conf`` file to the EFI partition we mounted earlier:

   .. code-block:: none

      # cp /usr/share/acrn/samples/nuc/acrn.conf /mnt/loader/entries/

   You will need to edit this file to adjust the kernel version (``linux`` section)
   and also insert the ``PARTUUID`` of your ``/dev/sda3`` partition
   (``root=PARTUUID=<><UUID of rootfs partition>``) in the ``options`` section.

   Use ``blkid`` to find out what your ``/dev/sda3`` ``PARTUUID`` value is.

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

Create a Network Bridge
=======================

Without a network bridge, the SOS and UOS are not able to talk to each
other.

A sample `bridge.sh
<https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/devicemodel/samples/nuc/bridge.sh>`__
is included in the Clear Linux release, and
is also available in the acrn-hypervisor/devicemodel GitHub repo (in the samples
folder) as shown here:

.. literalinclude:: ../../devicemodel/samples/nuc/bridge.sh
   :caption: devicemodel/samples/nuc/bridge.sh
   :language: bash

By default, the script is located in the ``/usr/share/acrn/samples/nuc/``
directory. Run it to create a network bridge:

.. code-block:: none

   # cd /usr/share/acrn/samples/nuc/
   # ./bridge.sh
   # cd

Set up Reference UOS
====================

#. On your NUC, download the pre-built reference Clear Linux UOS image into your
   (root) home directory:

   .. code-block:: none

      # cd ~
      # curl -O https://download.clearlinux.org/releases/22140/clear/clear-22140-kvm.img.xz

   .. note::
      In case you want to use or try out a newer version of Clear Linux as the UOS, you can
      download the latest from http://download.clearlinux.org/image. Make sure to adjust the steps
      described below accordingly (image file name and kernel modules version).

#. Uncompress it:

   .. code-block:: none

      # unxz clear-22140-kvm.img.xz

#. Deploy the UOS kernel modules to UOS virtual disk image (note: you'll need to use
   the same **standard** image version number noted in step 1 above):

   .. code-block:: none

      # losetup -f -P --show /root/clear-22140-kvm.img
      # mount /dev/loop0p3 /mnt
      # cp -r /usr/lib/modules/4.14.34-28.pk414-standard /mnt/lib/modules/
      # umount /mnt
      # sync

#. Edit and Run the launch_uos.sh script to launch the UOS.

   A sample `launch_uos.sh
   <https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/devicemodel/samples/nuc/launch_uos.sh>`__
   is included in the Clear Linux release, and
   is also available in the acrn-hypervisor/devicemodel GitHub repo (in the samples
   folder) as shown here:

   .. literalinclude:: ../../devicemodel/samples/nuc/launch_uos.sh
      :caption: devicemodel/samples/nuc/launch_uos.sh
      :language: bash
      :emphasize-lines: 22,24

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

There are two Device Manager memory allocation mechanisms available:

- Contiguous Memory Allocator (CMA), and 
- Huge Page Tables (HugeTLB).  HugeTLB is the default.

To choose CMA, do the following:

1) Add ``cma=reserved_mem_size@recommend_memory_offset-0``, (for example
   ``cma=2560M@0x100000000-0``) to the SOS cmdline in ``acrn.conf``

2) Start ``acrn-dm`` *without* the ``-T`` option

To support HugeTLB, do the following:

1) Do huge page reservation

   - For 1G huge page reservation, add ``hugepagesz=1G hugepages=reserved_pg_num``
     (for example, ``hugepagesz=1G hugepages=4``) to the SOS cmdline in
     ``acrn.conf`` (for EFI)

   - For 2M huge page reservation, after the SOS starts up, run the
     command::

        echo reserved_pg_num > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

   .. note::
      You can use 2M reserving method to do reservation for 1G page size, but it
      may fail.  For an EFI platform, you may skip 1G page reservation
      by using a 2M page, but make sure your huge page reservation size is
      large enough for your usage.

2)  Start ``acrn-dm`` *with* the ``-T`` option.

Build ACRN from Source
**********************

If you would like to build ACRN hypervisor and device model from source,
follow these steps.

Install build tools and dependencies
====================================

ARCN development is supported on popular Linux distributions,
each with their own way to install development tools:

* On a Clear Linux development system, install the ``os-clr-on-clr`` bundle to get
  the necessary tools:

  .. code-block:: console

     $ sudo swupd bundle-add os-clr-on-clr

* On a Ubuntu/Debian development system:

  .. code-block:: console

     $ sudo apt install git \
          gnu-efi \
          libssl-dev \
          libpciaccess-dev \
          uuid-dev \
          libsystemd-dev \
          libevent-dev \
          libxml2-dev \
          libusb-1.0-0-dev

  .. note::
     Ubuntu 14.04 requires ``libsystemd-journal-dev`` instead of ``libsystemd-dev``
     as indicated above.

* On a Fedora/Redhat development system:

  .. code-block:: console

     $ sudo dnf install gcc \
          gnu-efi-devel \
          libuuid-devel \
          openssl-devel \
          libpciaccess-devel \
          systemd-devel \
          libxml2-devel \
          libevent-devel \
          libusbx-devel


* On a CentOS development system:

  .. code-block:: console

     $ sudo yum install gcc \
             gnu-efi-devel \
             libuuid-devel \
             openssl-devel \
             libpciaccess-devel \
             systemd-devel \
             libxml2-devel \
             libevent-devel \
             libusbx-devel


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

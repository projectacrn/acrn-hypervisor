.. _getting_started:

Getting Started Guide
#####################

After reading the :ref:`introduction`, use this guide to get started
using ACRN in a reference setup.  We'll show how to set up your
development and target hardware, and then how to boot up of the ACRN
hypervisor and the `Clear Linux`_ Service OS and Guest OS on the Intel
|reg| NUC, using the UEFI BIOS.

.. _Clear Linux: https://clearlinux.org

Hardware setup
**************

The Intel |reg| NUC (NUC6CAYH) is the supported reference target
platform for ACRN work, as described in :ref:`hardware`, and is the only
platform tested with these setup instructions.

The recommended NUC hardware configuration is:

- NUC: `NUC Kit
  NUC6CAYH <https://www.intel.com/content/www/us/en/products/boards-kits/nuc/kits/nuc6cayh.html>`__
- Memory: 8G DDR3
- SSD: 120G SATA

In the software setup, we'll ensure the NUC is running the latest
(version 0047)
`UEFI BIOS <https://downloadcenter.intel.com/product/95062/Intel-NUC-Kit-NUC6CAYH>`__.

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

Currently, an installable version of ARCN does not exist. Therefore, we
need to setup a base Clear Linux OS to bootstrap ACRN. ACRN requires
Clear Linux version 21190 or newer.

1. Follow this `Clear Linux installation guide
   <https://clearlinux.org/documentation/clear-linux/get-started/bare-metal-install>`__
   as a starting point for installing Clear Linux onto your NUC.

2. At the "Choose Installation Type" screen, choose the "< Automatic >"
   option. This will install the minimum Clear Linux components.

3. At the "Choose target device for installation" screen, select your
   NUC's storage device to delete all existing content and create
   partitions automatically. The installer will
   create three partitions as shown in :numref:`CL-partitions`
   for the recommended 120GB SSD drive.

   .. table:: Clear Linux Partitions
      :widths: auto
      :name: CL-partitions

      +-------------+----------+-----------------------+
      | Partition   | Size     | Type                  |
      +=============+==========+=======================+
      | /dev/sda1   | 511M     | EFI System            |
      +-------------+----------+-----------------------+
      | /dev/sda2   | 32M      | Linux swap            |
      +-------------+----------+-----------------------+
      | /dev/sda3   | 111.3G   | Linux root (x86-64)   |
      +-------------+----------+-----------------------+

4. After installation is complete, boot into Clear Linux and log in as
   root and set a password.

5. Clear Linux is set to automatically update. If you do not wish for
   autoupdate, issue this command:

   .. code-block:: none

      # swupd autoupdate --disable

6. Use the ``swupd bundle-add`` command and add these Clear Linux bundles:

   .. table:: Clear Linux bundles
      :widths: auto
      :name: CL-bundles

      +--------------------+---------------------------------------------------+
      | Bundle             | Description                                       |
      +====================+===================================================+
      | vim                | vim text editor                                   |
      +--------------------+---------------------------------------------------+
      | curl               | Provide the curl command-line utility             |
      +--------------------+---------------------------------------------------+
      | network-basic      | Run network utilities and modify network settings |
      +--------------------+---------------------------------------------------+
      | service-os         | Add the acrn hypervisor, the acrn devicemodel and |
      |                    | Service OS kernel                                 |
      +--------------------+---------------------------------------------------+
      | kernel-pk          | Run the Intel "PK" kernel(product kernel source)  |
      |                    | and enterprise-style kernel with backports        |
      +--------------------+---------------------------------------------------+
      | os-clr-on-clr-dev  | Install dependent packages and libs               |
      +--------------------+---------------------------------------------------+

   .. code-block:: none

      # swupd bundle-add vim curl network-basic service-os kernel-pk os-clr-on-clr-dev

Add the ACRN hypervisor to the EFI Partition
============================================

In order to boot the ACRN SOS on the NUC, we need to add it to the EFI
partition. Follow these steps:

#. Mount the EFI partition and verify you have the following files:

   .. code-block:: none


    # mount /dev/sda1 /mnt

    # ls /mnt/EFI/org.clearlinux
    bootloaderx64.efi
    kernel-org.clearlinux.native.4.15.7-536
    kernel-org.clearlinux.pk414-sos.4.14.23-19
    kernel-org.clearlinux.pk414-standard.4.14.23-19
    loaderx64.efi

#. Copy the ``acrn.efi`` hypervisor application to the EFI partition.

   .. code-block:: none

      # cp /usr/share/acrn.efi /mnt/EFI/org.clearlinux

#. Create a boot entry for ACRN. It must contain these settings:

   +-----------+----------------------------------------------------------------+
   | Setting   | Description                                                    |
   +===========+================================================================+
   | title     | Text to show in the boot menu                                  |
   +-----------+----------------------------------------------------------------+
   | efi       | Executable EFI image                                           |
   +-----------+----------------------------------------------------------------+
   | options   | Options to pass to the EFI program or kernel boot parameters   |
   +-----------+----------------------------------------------------------------+

   A sample `acrn.conf
   <https://github.com/projectacrn/acrn-hypervisor/tree/master/bsp/uefi/clearlinux/acrn.conf>`__
   is available in the acrn-hypervisor GitHub repo (in the bsp/uefi/clearlinux
   folder) as shown here:

   .. literalinclude:: ../../acrn-hypervisor/bsp/uefi/clearlinux/acrn.conf
      :caption: acrn-hypervisor/bsp/uefi/clearlinux/acrn.conf

   You can download ``acrn.conf`` and copy it to
   ``/mnt/loader/entries/acrn.conf`` and use it directly.  If you're following
   the instructions above, the partition (``root=/dev/sda3``) and image
   locatations will match.

#. Add a timeout period for Systemd-Boot to wait, otherwise it will not
   present the boot menu and will always boot the base Clear Linux
   kernel.

   .. code-block:: none

      # clr-boot-manager set-timeout 20
      # clr-boot-manager update

#. Reboot and select "The ACRN Hypervisor" to boot, as shown in
   :numref:`gsg-bootmenu`:

   .. figure:: images/gsg-bootmenu.png
      :align: center
      :name: gsg-bootmenu

      ACRN Hypervisor Boot menu

#. After booting up the ACRN hypervisor, the Service OS be launched
   automatically by default, as shown in :numref:`gsg-sos-console`:

   .. figure:: images/gsg-sos-console.png
      :align: center
      :name: gsg-sos-console

      Service OS Console

#. From here you can login as root and set the first-time root password.

Create a Network Bridge
=======================

Without a network bridge, the SOS and UOS are not able to talk to each
other.

A sample `bridge.sh
<https://github.com/projectacrn/acrn-devicemodel/tree/master/samples/bridge.sh>`__
is available in the acrn-devicemodel GitHub repo (in the samples
folder) as shown here:

.. literalinclude:: ../../acrn-devicemodel/samples/bridge.sh
   :caption: acrn-devicemodel/samples/bridge.sh
   :language: bash

You can download ``bridge.sh``, copy it to your root home
directory, and use it directly by making it executable and running it:

.. code-block:: none

   # chmod +x ./bridge.sh
   # ./bridge.sh

Set up Reference UOS
====================

#. Download the pre-built reference Clear Linux UOS image.

   .. code-block:: none

      # curl -O https://download.clearlinux.org/image/clear-21180-kvm.img.xz

#. Uncompress it.

   .. code-block:: none

      # unxz clear-21180-kvm.img.xz

#. Deploy the UOS kernel modules to UOS virtual disk image

   .. code-block:: none

      # losetup -f -P --show /root/clear-21180-kvm.img
      # ls /dev/loop0*
      # mount /dev/**loop0p3** /mnt
      # cp -r /usr/lib/doc/modules/4.14.23-19.pk414-standard /mnt/lib/doc/modules/
      # umount /mnt
      # sync

#. Create the launch_uos.sh script and run it to launch the UOS.
   A sample `launch_uos.sh
   <https://github.com/projectacrn/acrn-devicemodel/tree/master/samples/launch_uos.sh>`__
   is available in the acrn-devicemodel GitHub repo (in the samples
   folder) as shown here:

   .. literalinclude:: ../../acrn-devicemodel/samples/launch_uos.sh
      :caption: acrn-devicemodel/samples/launch_uos.sh
      :language: bash

   You can download ``launch_uos.sh``, copy it to your root home
   directory, and use it directly by making it executable and running it:

   .. code-block:: none

      # chmod +x ./launch_uos.sh
      # ./launch_uos.sh

#. At this point, you've successfully booted the ACRN system,
   hypervisor, SOS and UOS:

   .. figure:: images/gsg-successful-boot.png
      :align: center
      :name: gsg-successful-boot


Build ACRN from Source
**********************

If you would like to build ACRN hypervisor and device model from source,
follow these steps:

#. On your Clear Linux system, install the os-clr-on-clr bundle to get
   the necessary tools.

   .. code-block:: none

      # swupd bundle-add os-clr-on-clr

#. Download the ACRN hypervisor and build it.

   .. code-block:: none

      # git clone https://github.com/projectacrn/acrn-hypervisor
      # cd acrn-hypervisor
      # make PLATFORM=uefi

   The build results are found in the ``build`` directory.

#. Download the ACRN device model and build it.

   .. code-block:: none

      # git clone https://github.com/projectacrn/acrn-devicemodel
      # cd acrn-devicemodel
      # make

   The build results are found in the ``build`` directory.

Follow the same instructions to boot and test the images you created
from your build.

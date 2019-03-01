.. _getting-started-apl-nuc:

Getting started guide for Intel NUC
###################################

The Intel |reg| NUC (NUC6CAYH) is the primary tested
platform for ACRN development, and its setup is described below.


Hardware setup
**************

Two Apollo Lake Intel platforms, described in :ref:`hardware`, are currently
supported for ACRN development:

- The `UP Squared board
  <https://www.up-board.org/upsquared/specifications/>`_ (UP2) is also
  known to work and its setup is described in :ref:`getting-started-up2`.

Firmware update on the NUC
==========================

You may need to update to the latest UEFI firmware for the NUC hardware.
Follow these `BIOS Update Instructions
<https://www.intel.com/content/www/us/en/support/articles/000005636.html>`__
for downloading and flashing an updated BIOS for the NUC.

Software setup
**************

.. _set-up-CL:

Set up a Clear Linux Operating System
=====================================

Currently, an installable version of ARCN does not exist. Therefore, you
need to setup a base Clear Linux OS and you'll build and bootstrap ACRN
on your platform. You'll need a network connection for your platform to
complete this setup.

.. note::

   Please refer to the ACRN :ref:`release_notes` for the Clear Linux OS
   version number tested with a specific ACRN release.  Adjust the
   instruction below to reference the appropriate version number of Clear
   Linux OS (we use version 27960 as an example).

#. Download the compressed Clear Linux OS installer image from
   https://download.clearlinux.org/releases/27960/clear/clear-27960-installer.img.xz
   and follow the `Clear Linux OS installation guide
   <https://clearlinux.org/documentation/clear-linux/get-started/bare-metal-install>`__
   as a starting point for installing Clear Linux OS onto your platform.  Follow the recommended
   options for choosing an **Manual (Advanced)** installation type, and using the platform's
   storage as the target device for installation (overwriting the existing data
   and creating three partitions on the platform's storage drive).

   High-level steps should be:

   #.  Install Clear Linux OS on a NUC using the "Manual (Advanced)" option.
   #.  Use default partition scheme for storage
   #.  Name the host "clr-sos-guest"
   #.  Add an administrative user "clear" with "sudoers" privilege
   #.  Add these additional bundles "editors", "user-basic", "desktop-autostart", "network-basic"
   #.  For network, choose “DHCP”

#. After installation is complete, boot into Clear Linux OS, login as
   **clear**, and set a password.

#. Clear Linux OS is set to automatically update itself. We recommend that you disable
   this feature to have more control over when the updates happen. Use this command
   to disable the autoupdate feature:

   .. code-block:: none

      $ sudo swupd autoupdate --disable

   .. note::
      The Clear Linux OS installer will automatically check for updates and install the
      latest version available on your system. If you wish to use a specific version
      (such as 27960), you can achieve that after the installation has completed using
      ``sudo swupd verify --fix --picky -m 27960``

#. If you have an older version of Clear Linux OS already installed
   on your hardware, use this command to upgrade Clear Linux OS
   to version 27960 (or newer):

   .. code-block:: none

      $ sudo swupd update -m 27960     # or newer version

#. Use the ``sudo swupd bundle-add`` command and add these Clear Linux OS bundles:

   .. code-block:: none

      $ sudo swupd bundle-add service-os kernel-iot-lts2018

   .. table:: Clear Linux OS bundles
      :widths: auto
      :name: CL-bundles

      +--------------------+---------------------------------------------------+
      | Bundle             | Description                                       |
      +====================+===================================================+
      | service-os         | Add the acrn hypervisor, the acrn devicemodel and |
      |                    | Service OS kernel                                 |
      +--------------------+---------------------------------------------------+
      | kernel-iot-lts2018 | Run the Intel kernel "kernel-iot-lts2018"         |
      |                    | which is enterprise-style kernel with backports   |
      +--------------------+---------------------------------------------------+

.. _add-acrn-to-efi:

Add the ACRN hypervisor to the EFI Partition
============================================

In order to boot the ACRN SOS on the platform, you'll need to add it to the EFI
partition. Follow these steps:

#. Mount the EFI partition and verify you have the following files:

   .. code-block:: none

      $ sudo ls -1 /boot/EFI/org.clearlinux
      bootloaderx64.efi
      kernel-org.clearlinux.native.4.20.11-702
      kernel-org.clearlinux.iot-lts2018-sos.4.19.23-19
      kernel-org.clearlinux.iot-lts2018.4.19.23-19
      loaderx64.efi

   .. note::
      On Clear Linux OS, the EFI System Partion (e.g.: ``/dev/sda1``) is mounted under ``/boot`` by default
      The Clear Linux project releases updates often, sometimes
      twice a day, so make note of the specific kernel versions (*iot-lts2018 and *iot-lts2018-sos*) listed on your system,
      as you will need them later.

   .. note::
      The EFI System Partition (ESP) may be different based on your hardware.
      It will typically be something like ``/dev/mmcblk0p1`` on platforms
      that have an on-board eMMC or ``/dev/nvme0n1p1`` if your system has
      a non-volatile storage media attached via a PCI Express (PCIe) bus
      (NVMe).

#. Put the ``acrn.efi`` hypervisor application (included in the Clear
   Linux OS release) on the EFI partition with:

   .. code-block:: none

      $ sudo mkdir /boot/EFI/acrn
      $ sudo cp /usr/lib/acrn/acrn.efi /boot/EFI/acrn/

#. Configure the EFI firmware to boot the ACRN hypervisor by default

   The ACRN hypervisor (``acrn.efi``) is an EFI executable
   loaded directly by the platform EFI firmware. It then in turns loads the
   Service OS bootloader. Use the ``efibootmgr`` utility to configure the EFI
   firmware and add a new entry that loads the ACRN hypervisor.

   .. code-block:: none

      $ sudo efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L "ACRN"

   .. note::

      Be aware that a Clear Linux OS update that includes a kernel upgrade will
      reset the boot option changes you just made. A Clear Linux OS update could
      happen automatically (if you have not disabled it as described above),
      if you later install a new bundle to your system, or simply if you
      decide to trigger an update manually. Whenever that happens,
      double-check the platform boot order using ``efibootmgr -v`` and
      modify it if needed.

   The ACRN hypervisor (``acrn.efi``) accepts three command-line parameters that
   tweak its behaviour:

   1. ``bootloader=``: this sets the EFI executable to be loaded once the hypervisor
      is up and running. This is typically the bootloader of the Service OS and the
      default value is to use the Clear Linux OS bootloader, i.e.:
      ``\EFI\org.clearlinux\bootloaderx64.efi``.
   #. ``uart=``: this tells the hypervisor where the serial port (UART) is found or
      whether it should be disabled. There are three forms for this parameter:

      #. ``uart=disabled``: this disables the serial port completely
      #. ``uart=bdf@<BDF value>``:  this sets the PCI serial port based on its BDF.
         For example, use ``bdf@0:18.1`` for a BDF of 0:18.1 ttyS1.
      #. ``uart=port@<port address>``: this sets the serial port address
   #. ``vuart=ttySn@irqN``: this tells the hypervisor which virtual serial device SOS
      will use and its IRQ number. This is used to avoid conflict with SOS passthrough  
      devices' interrupt. If UART is set to ttyS1, and its native IRQ is 5, you'd better
      set ``vuart=ttyS1@irq5`` (Use 'dmesg | grep tty' to get IRQ information).
      Also set ``console=ttyS1`` in ``acrn.conf`` to match the SOS boot args.

   Here is a more complete example of how to configure the EFI firmware to load the ACRN
   hypervisor and set these parameters.

   .. code-block:: none

      $ sudo efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L "ACRN NUC Hypervisor" \
            -u "bootloader=\EFI\org.clearlinux\bootloaderx64.efi uart=disabled"

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

   A starter acrn.conf configuration file is included in the Clear Linux
   OS release and is
   also available in the acrn-hypervisor/hypervisor GitHub repo as `acrn.conf
   <https://github.com/projectacrn/acrn-hypervisor/blob/master/efi-stub/clearlinux/acrn.conf>`__
   as shown here:

   .. literalinclude:: ../../efi-stub/clearlinux/acrn.conf
      :caption: efi-stub/clearlinux/acrn.conf

   On the platform, copy the ``acrn.conf`` file to the EFI partition we mounted earlier:

   .. code-block:: none

      $ sudo cp /usr/share/acrn/samples/nuc/acrn.conf /boot/loader/entries/

   You will need to edit this file to adjust the kernel version (``linux`` section),
   insert the ``PARTUUID`` of your ``/dev/sda3`` partition
   (``root=PARTUUID=<UUID of rootfs partition>``) in the ``options`` section, and
   add the ``hugepagesz=1G hugepages=2`` at end of the ``options`` section.

   Use ``blkid`` to find out what your ``/dev/sda3`` ``PARTUUID`` value is. Here
   is a handy one-line command to do that:

   .. code-block:: none

      # sed -i "s/<UUID of rootfs partition>/`blkid -s PARTUUID -o value \
                     /dev/sda3`/g" /boot/loader/entries/acrn.conf

   .. note::
      It is also possible to use the device name directly, e.g. ``root=/dev/sda3``

#. Add a timeout period for Systemd-Boot to wait, otherwise it will not
   present the boot menu and will always boot the base Clear Linux OS

   .. code-block:: none

      $ sudo clr-boot-manager set-timeout 20
      $ sudo clr-boot-manager update


#. Reboot and select "The ACRN Service OS" to boot, as shown below:


   .. code-block:: console
      :emphasize-lines: 1
      :caption: ACRN Service OS Boot Menu

      => The ACRN Service OS
      Clear Linux OS for Intel Architecture (Clear-linux-iot-lts2018-4.19.23-19)
      Clear Linux OS for Intel Architecture (Clear-linux-iot-lts2018-sos-4.19.23-19)
      Clear Linux OS for Intel Architecture (Clear-linux-native.4.20.11-702)
      EFI Default Loader
      Reboot Into Firmware Interface

#. After booting up the ACRN hypervisor, the Service OS will be launched
   automatically by default, and the Clear Linux OS desktop will be showing with user "clear",
   (or you can login remotely with an "ssh" client).
   If there is any issue which makes the GNOME desktop doesn't show successfully, then the system will go to
   shell console.

#. From ssh client, login as user "clear" using the password you set previously when
   you installed Clear Linux OS.

#. After rebooting the system, check that the ACRN hypervisor is running properly with:

  .. code-block:: none

   $ dmesg | grep ACRN
   [    0.000000] Hypervisor detected: ACRN
   [    1.687128] ACRNTrace: acrn_trace_init, cpu_num 4
   [    1.693129] ACRN HVLog: acrn_hvlog_init

If you see log information similar to this, the ACRN hypervisor is running properly
and you can start deploying a User OS.  If not, verify the EFI boot options, SOS
kernel, and ``acrn.conf`` settings are correct (as described above).


ACRN Network Bridge
===================

ACRN bridge has been setup as a part of systemd services for device communication. The default
bridge creates ``acrn_br0`` which is the bridge and ``acrn_tap0`` as an initial setup. The files can be
found in ``/usr/lib/systemd/network``. No additional setup is needed since systemd-networkd is
automatically enabled after a system restart.

Set up Reference UOS
====================

#. On your platform, download the pre-built reference Clear Linux OS UOS
   image version 27960 (or newer) into your (root) home directory:

   .. code-block:: none

      $ cd ~
      $ mkdir uos
      $ cd uos
      $ curl https://download.clearlinux.org/releases/27960/clear/clear-27960-kvm.img.xz -o uos.img.xz

   .. note::
      In case you want to use or try out a newer version of Clear Linux OS as the UOS, you can
      download the latest from http://download.clearlinux.org/image. Make sure to adjust the steps
      described below accordingly (image file name and kernel modules version).

#. Uncompress it:

   .. code-block:: none

      $ unxz uos.img.xz

#. Deploy the UOS kernel modules to UOS virtual disk image (note: you'll need to use
   the same **iot-lts2018** image version number noted in step 1 above):

   .. code-block:: none

      $ sudo losetup -f -P --show uos.img
      $ sudo mount /dev/loop0p3 /mnt
      $ sudo cp -r /usr/lib/modules/"`readlink /usr/lib/kernel/default-iot-lts2018 | awk -F '2018.' '{print $2}'`.iot-lts2018" /mnt/lib/modules
      $ sudo umount /mnt
      $ sync

#. Edit and Run the ``launch_uos.sh`` script to launch the UOS.

   A sample `launch_uos.sh
   <https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/devicemodel/samples/nuc/launch_uos.sh>`__
   is included in the Clear Linux OS release, and
   is also available in the acrn-hypervisor/devicemodel GitHub repo (in the samples
   folder) as shown here:

   .. literalinclude:: ../../devicemodel/samples/nuc/launch_uos.sh
      :caption: devicemodel/samples/nuc/launch_uos.sh
      :language: bash

   By default, the script is located in the ``/usr/share/acrn/samples/nuc/``
   directory. You can run it to launch the User OS:

   .. code-block:: none

      $ cd /usr/share/acrn/samples/nuc/
      $ sudo ./launch_uos.sh

#. At this point, you've successfully booted the ACRN hypervisor,
   SOS, and UOS:

   .. figure:: images/gsg-successful-boot.png
      :align: center
      :name: gsg-successful-boot


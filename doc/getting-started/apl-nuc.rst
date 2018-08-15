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

   ACRN v0.1 (and the current master branch) requires Clear Linux
   version 23690 or newer.  If you use a newer version of Clear Linux,
   you'll need to adjust the instructions below to reference the version
   number of Clear Linux you are using.

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

   .. code-block:: console

      # swupd autoupdate --disable

#. If you have an older version of Clear Linux already installed
   on your hardware, use this command to upgrade Clear Linux
   to version 23690 (or newer):

   .. code-block:: console

      # swupd update -m 23690     # or newer version

#. Use the ``swupd bundle-add`` command and add these Clear Linux bundles:

   .. code-block:: console

      # swupd bundle-add vim network-basic service-os kernel-pk \
        desktop openssh-server

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
      | desktop            | Graphical desktop application, with Weston        |
      |                    | compositing window manager included               |
      +--------------------+---------------------------------------------------+
      | openssh-server     | Server-side support for secure connectivity and   |
      |                    | remote login using the SSH protocol               |
      +--------------------+---------------------------------------------------+

Add the ACRN hypervisor to the EFI Partition
============================================

In order to boot the ACRN SOS on the platform, you'll need to add it to the EFI
partition. Follow these steps:

#. Mount the EFI partition and verify you have the following files:

   .. code-block:: console

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

   .. code-block:: console

      # mkdir /mnt/EFI/acrn
      # cp /usr/lib/acrn/acrn.efi /mnt/EFI/acrn/

#. Configure the EFI firmware to boot the ACRN hypervisor by default

   The ACRN hypervisor (``acrn.efi``) is an EFI executable
   loaded directly by the platform EFI firmware. It then in turns loads the
   Service OS bootloader. Use the ``efibootmgr`` utility to configure the EFI
   firmware and add a new entry that loads the ACRN hypervisor.

   .. code-block:: console

      # efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L "ACRN"

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

   .. code-block:: console

      # efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L "ACRN NUC Hypervisor" \
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

   A starter acrn.conf configuration file is included in the Clear Linux release and is
   also available in the acrn-hypervisor/hypervisor GitHub repo as `acrn.conf
   <https://github.com/projectacrn/acrn-hypervisor/hypervisor/tree/master/bsp/uefi/clearlinux/acrn.conf>`__
   as shown here:

   .. literalinclude:: ../../hypervisor/bsp/uefi/clearlinux/acrn.conf
      :caption: hypervisor/bsp/uefi/clearlinux/acrn.conf

   On the platform, copy the ``acrn.conf`` file to the EFI partition we mounted earlier:

   .. code-block:: console

      # cp /usr/share/acrn/samples/nuc/acrn.conf /mnt/loader/entries/

   You will need to edit this file to adjust the kernel version (``linux`` section),
   insert the ``PARTUUID`` of your ``/dev/sda3`` partition
   (``root=PARTUUID=<><UUID of rootfs partition>``) in the ``options`` section, and
   add the ``hugepagesz=1G hugepages=2`` at end of the ``options`` section.

   Use ``blkid`` to find out what your ``/dev/sda3`` ``PARTUUID`` value is.

   .. note::
      It is also possible to use the device name directly, e.g. ``root=/dev/sda3``

#. Add a timeout period for Systemd-Boot to wait, otherwise it will not
   present the boot menu and will always boot the base Clear Linux

   .. code-block:: console

      # clr-boot-manager set-timeout 20
      # clr-boot-manager update

#. Add new user

   .. code-block:: console

      # useradd cl-sos
      # passwd cl-sos
      # usermod -G wheel -a cl-sos

#. Enable weston service

   .. code-block:: console

      # systemctl enable weston@cl-sos
      # systemctl start weston@cl-sos

#. Reboot and select "The ACRN Service OS" to boot, as shown below:


   .. code-block:: console
      :emphasize-lines: 1
      :caption: ACRN Service OS Boot Menu

      => The ACRN Service OS
      Clear Linux OS for Intel Architecture (Clear-linux-native-4.17.6.590)
      Clear Linux OS for Intel Architecture (Clear-linux-pk414-sos-4.14.52.63)
      Clear Linux OS for Intel Architecture (Clear-linux-pk414-standard-4.14.52.63)
      EFI Default Loader
      Reboot Into Firmware Interface

#. After booting up the ACRN hypervisor, the Service OS will be launched
   automatically by default, as shown here:

   .. code-block:: console
      :caption: Service OS Console

      clr-7259a7c5bbdd4bcaa9a59d5841b4ace login: root
      You are required to change your password immediately (administrator enforced)
      New password:
      Retype new password:
      root@clr-7259a7c5bbdd4bcaa9a59d5841b4ace ~ # _

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

#. On your platform, download the pre-built reference Clear Linux UOS
   image version 23690 (or newer) into your (root) home directory:

   .. code-block:: none

      # cd ~
      # curl -O https://download.clearlinux.org/releases/23690/clear/clear-23690-kvm.img.xz

   .. note::
      In case you want to use or try out a newer version of Clear Linux as the UOS, you can
      download the latest from http://download.clearlinux.org/image. Make sure to adjust the steps
      described below accordingly (image file name and kernel modules version).

#. Uncompress it:

   .. code-block:: console

      # unxz clear-23690-kvm.img.xz

#. Deploy the UOS kernel modules to UOS virtual disk image (note: you'll need to use
   the same **standard** image version number noted in step 1 above):

   .. code-block:: console

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
      (``clear-23690-kvm.img.xz``), you will need to modify the Clear Linux file name
      and version number highlighted above (the ``-s 3,virtio-blk`` argument) to match
      what you have downloaded above. Likewise, you may need to adjust the kernel file
      name on the second line highlighted (check the exact name to be used using:
      ``ls /usr/lib/kernel/org.clearlinux*-standard*``).

   By default, the script is located in the ``/usr/share/acrn/samples/nuc/``
   directory. You can edit it there, and then run it to launch the User OS:

   .. code-block:: console

      # cd /usr/share/acrn/samples/nuc/
      # ./launch_uos.sh

#. At this point, you've successfully booted the ACRN hypervisor,
   SOS, and UOS:

   .. figure:: images/gsg-successful-boot.png
      :align: center
      :name: gsg-successful-boot


USB Device Sharing
==========================================

The ACRN hypervisor supports USB device sharing.  Suppose you have
two keyboards and mice connected to your device, one keyboard and 
mouse set for the SOS, and the other set for the UOS.  

1. Boot the SOS and plug in the two keyboards and two mice
   into four available USB ports on the device (the NUC we recommend has 4 USB ports).

#. Run ``dmesg`` to find the kernel messages logging the enumeration
   of the connected keyboards and mice.  For example::

  .. code-block:: console

      # dmesg
      [  560.469525] usb 1-4: Product: USB Optical Mouse
      [  560.469600] usb 1-4: Manufacturer: Logitech
      [  560.472238] input: Logitech USB Optical Mouse as /devices/pci0000:00/0000:00:14.0/usb1/1-4/1- 4:1.0/0003:046D:C018.0005/input/input8
      [  560.472673] hid-generic 0003:046D:C018.0005: input,hidraw1: USB HID v1.11 Mouse [Logitech USB Optical Mouse] on usb-   0000:00:14.0-4/input0
      [  561.743470] usb 1-3: USB disconnect, device number 6
      [  565.504044] usb 1-3: new low-speed USB device number 8 using xhci_hcd
      [  565.639056] usb 1-3: New USB device found, idVendor=03f0, idProduct=0024
      [  565.639167] usb 1-3: New USB device strings: Mfr=1, Product=2, SerialNumber=0
      [  565.639282] usb 1-3: Product: HP Basic USB Keyboard
      [  565.639362] usb 1-3: Manufacturer: CHICONY
      [  565.644013] input: CHICONY HP Basic USB Keyboard as /devices/pci0000:00/0000:00:14.0/usb1/1-3/1- 3:1.0/0003:03F0:0024.0006/input/input9
      [  565.696139] hid-generic 0003:03F0:0024.0006: input,hidraw0: USB HID v1.10 Keyboard [CHICONY HP Basic USB Keyboard] on usb-0000:00:14.0-3/input0
      [ 1000.587071] usb 1-2: new low-speed USB device number 9 using xhci_hcd
      [ 1000.719824] usb 1-2: New USB device found, idVendor=046d, idProduct=c315
      [ 1000.719934] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=0
      [ 1000.720048] usb 1-2: Product: Logitech USB Keyboard
      [ 1000.720143] usb 1-2: Manufacturer: Logitech
      [ 1000.724286] input: Logitech Logitech USB Keyboard as /devices/pci0000:00/0000:00:14.0/usb1/1-2/1-2:1.0/0003:046D:C315.0007/input/input10
      [ 1000.776433] hid-generic 0003:046D:C315.0007: input,hidraw2: USB HID v1.10 Keyboard [Logitech Logitech USB Keyboard] on usb-0000:00:14.0-2/input0
      [ 1008.387071] usb 1-1: new low-speed USB device number 10 using xhci_hcd
      [ 1008.516312] usb 1-1: New USB device found, idVendor=046d, idProduct=c077
      [ 1008.516421] usb 1-1: New USB device strings: Mfr=1, Product=2, SerialNumber=0
      [ 1008.516536] usb 1-1: Product: USB Optical Mouse
      [ 1008.516610] usb 1-1: Manufacturer: Logitech
      [ 1008.519459] input: Logitech USB Optical Mouse as /devices/pci0000:00/0000:00:14.0/usb1/1-1/1-        1:1.0/0003:046D:C077.0008/input/input11
      [ 1008.519714] hid-generic 0003:046D:C077.0008: input,hidraw3: USB HID v1.11 Mouse [Logitech USB Optical Mouse] on usb-   0000:00:14.0-1/input0

#. From ``dmesg`` info, you can easly find specific USB device info connected to certain port by pluging in 
   USB device one by one 
   for example::
    
      mouse #1: usb 1-1
      keyboard #1: usb 1-2 
      keyboard #2: usb 1-3
      mouse #2: usb 1-4

#. Let's assign keyboard #1 and mouse #1 to the UOS. Use a text editor to modify
   ``/usr/share/acrn/samples/nuc/launch_uos.sh`` and add the line (using
   the keyboard and mouse identified in your dmesg output)::

      -s 9, xhci, 1-1:1-2 \

   Save the file, exit the editor, and run the ``sync`` command to ensure
   any pending write buffers are written to disk. 
  
   In our example, keyboard #2 and mouse #2 will be used to interact with the Service OS (SOS) 
   and the keyboard #1 and mouse #1 will be used for the User OS (UOS).
   
   .. note::
      You may have to unplug and plug the keyboard and mouse back in (same connectors!)
      assigned to the UOS after launching the UOS.  


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
          python3-pip \
          libblkid-dev \
          e2fslibs-dev
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
          python3-pip \
          libblkid-devel \
          e2fsprogs-devel
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

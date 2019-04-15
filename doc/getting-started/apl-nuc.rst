.. _getting-started-apl-nuc:

Getting started guide for Intel NUC
###################################

The Intel |reg| NUC is the primary tested platform for ACRN development,
and its setup is described below.


Hardware setup
**************

Intel Apollo Lake NUC (APL) and Intel Kaby Lake NUC (KBL),
described in :ref:`hardware`, are currently supported for ACRN development:

- We can enable the serial console on `KBL
  <https://www.amazon.com/dp/B07D5HHMSB/ref=cm_sw_r_cp_ep_dp_hXm0BbBV0CER3>`__
  (NUC7i5DNHE), but this is not supported on APL (NUC6CAYH).


Connecting to the serial port
=============================

If you don't need a serial console you can ignore this section. If you're
using a KBL NUC and you need a serial console, you'll need to prepare
an RS232 cable to connect to the KBL NUC serial port header, as shown
below:

.. figure:: images/KBL-serial-port-header.png
   :align: center

   You can refer to the `'Technical Product Specification'
   <https://www.intel.com/content/dam/support/us/en/documents/boardsandkits/NUC7i5DN_TechProdSpec.pdf>`__
   for details


.. figure:: images/KBL-serial-port-header-to-RS232-cable.jpg
   :align: center

   KBL serial port header to RS232 `cable
   <https://www.amazon.com/dp/B07BV1W6N8/ref=cm_sw_r_cp_ep_dp_wYm0BbABD5AK6>`_


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

Currently, an installable version of ACRN does not exist. Therefore, you
need to setup a base Clear Linux OS and you'll build and bootstrap ACRN
on your platform. You'll need a network connection for your platform to
complete this setup.

.. note::

   Please refer to the ACRN :ref:`release_notes` for the Clear Linux OS
   version number tested with a specific ACRN release.  Adjust the
   instruction below to reference the appropriate version number of Clear
   Linux OS (we use version 28960 as an example).

#. Download the compressed Clear Linux OS installer image from
   https://download.clearlinux.org/releases/28960/clear/clear-28960-installer.img.xz
   and follow the `Clear Linux OS installation guide
   <https://clearlinux.org/documentation/clear-linux/get-started/bare-metal-install>`__
   as a starting point for installing Clear Linux OS onto your platform. Follow the recommended
   options for choosing an **Advanced options** installation type, and using the platform's
   storage as the target device for installation (overwriting the existing data
   and creating three partitions on the platform's storage drive).

   High-level steps should be:

   #.  Launch the Clear Linux OS installer boot menu
   #.  With Clear Linux OS highlighted, select Enter.
   #.  From the Main Menu, select "Configure Media" and set "Auto Partition" to your desired hard disk.
   #.  ``shift + A`` to the "Advanced options".
   #.  Select "Additional Bundle Selection" to add additional bundles "desktop-autostart", "editors", "network-basic", "user-basic"
   #.  Select "User Manager" to add an administrative user "clear"
   #.  Select "Assign Hostname" to set the hostname as "clr-sos-guest"

#. After installation is complete, boot into Clear Linux OS, login as
   **clear**, and set a password.

#. The remaining instructions below provide detailed instructions on setting
   up the ACRN Hypervisor, Service OS, and Guest OS.  We also provide an
   automated script that does all these steps for you, so you can skip these
   manual steps.  See the `quick-setup-guide`_ section below to use the
   automated setup script.

.. _quick-setup-guide:

Use the script to set up ACRN automatically
===========================================

It is little complicate to setup the SOS or UOS, so we provide a script to do it quickly and automatically.
You can find the script `here
<https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/doc/getting-started/acrn_quick_setup.sh>`__
and please note that should be run with root privilege since it will modify various system parameters.

#. Installing Clear Linux and login system

#. Open a terminal

#. Download ``acrn_quick_setup.sh`` script to set up the SOS. If you don't need a proxy to
   get the script, you can just skip the ``export`` command.

   .. code-block:: console

      $ export https_proxy=https://myproxy.mycompany.com:port
      $ cd ~ && wget https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/doc/getting-started/acrn_quick_setup.sh
      $ sudo sh acrn_quick_setup.sh -s 28960
      Password:
      Upgrading SOS...
      Disable auto update...
      Clear Linux version 28960 is already installed. Continuing to setup SOS...
      Adding the service-os and kernel-iot-lts2018 bundles...
        ...100%
        ...100%
        ...100%
      none
      Add /mnt/EFI/acrn folder
      Copy /usr/share/acrn/samples/nuc/acrn.conf /mnt/loader/entries/
      Copy /usr/lib/acrn/acrn.efi to /mnt/EFI/acrn
      Check ACRN efi boot event
      Clean all ACRN efi boot event
      Check linux bootloader event
      Clean all Linux bootloader event
      Add new ACRN efi boot event
      Create loader.conf
      Add default (5 seconds) boot wait time
      Add default boot to ACRN
      Getting latest Service OS kernel version: kernel-org.clearlinux.iot-lts2018-sos.4.19.34-45
      Getting current Service OS kernel version: kernel-org.clearlinux.iot-lts2018-sos.4.19.13-1901141830
      Replacing root partition uuid in acrn.conf
      Replace with new SOS kernel in acrn.conf
      Service OS setup done!
      Rebooting Service OS to take effects.
      Rebooting.

   .. note::
      This script is using ``/dev/sda1`` as default EFI System Partition (ESP). The ESP
      may be different based on your hardware and then you should specify it directly with ``-e`` option.
      Here is an example for setup SOS on NVMe SSD: ``sudo sh acrn_quick_setup.sh -s 28960 -e /dev/nvme0n1p1``

   .. note::
      If you don't need reboot automatically after set up SOS, then you should run this command:
      ``sudo sh acrn_quick_setup.sh -s 28960 -d``

#. After the system reboots and login as the clear user, you may need to check the ``dmesg`` to make sure
   the SOS is boot successfully.

   .. code-block:: console

      $ dmesg | grep ACRN
      [    0.000000] Hypervisor detected: ACRN
      [    1.220887] ACRNTrace: Initialized acrn trace module with 4 cpu
      [    1.224401] ACRN HVLog: Initialized hvlog module with 4 cpu

#. If you want to continue to set up a Guest OS after boot SOS, then you can run
   ``sudo sh acrn_quick_setup.sh -u 28960`` to get your UOS ready.

   .. code-block:: console

      $ sudo sh acrn_quick_setup.sh -u 28960
      Password:
      Upgrading UOS...
      Downloading UOS image: https://download.clearlinux.org/releases/28960/clear/clear-28960-kvm.img.xz
        % Total    % Received % Xferd  Average Speed   Time    Time     Time  Current
                                       Dload  Upload   Total   Spent    Left  Speed
       14  248M   14 35.4M    0     0   851k      0  0:04:57  0:00:42  0:04:15  293k

   After download is completed, you'll get this output.

   .. code-block:: console

      Unxz UOS image: clear-28960-kvm.img.xz
      Get UOS image: clear-28960-kvm.img
      Upgrade UOS done...
      Now you can run this command to start UOS...
      $ sudo /root/launch_uos_28960.sh

   .. note::
      If you have a local UOS image which is named ``clear-28960-kvm.img.xz`` or it's just uncompressed into
      ``/root`` folder which is named ``clear-28960-kvm.img``, then you can run
      ``sudo sh acrn_quick_setup.sh -u 28960 -k`` to skip downloading it again and set up UOS directly.

#. Now you can run ``sudo /root/launch_uos_28960.sh`` to launch UOS.

   .. code-block:: console

      $ sudo /root/launch_uos_28960.sh
      Password:
      cpu1 online=0
      cpu2 online=0
      cpu3 online=0
      passed gvt-g optargs low_gm 64, high_gm 448, fence 8
      SW_LOAD: get kernel path /usr/lib/kernel/default-iot-lts2018
      SW_LOAD: get bootargs root=/dev/vda3 rw rootwait maxcpus=1 nohpet console=tty0 console=hvc0   console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M   consoleblank=0 tsc=reliable i915.avail_planes_per_pipe=0x070F00   i915.enable_hangcheck=0 i915.nuclear_pageflip=1 i915.enable_guc_loading=0   i915.enable_guc_submission=0 i915.enable_guc=0
      VHM api version 1.0
      open hugetlbfs file /run/hugepage/acrn/huge_lv1/D279543825D611E8864ECB7A18B34643
      open hugetlbfs file /run/hugepage/acrn/huge_lv2/D279543825D611E8864ECB7A18B34643
      level 0 free/need pages:512/0 page size:0x200000
      level 1 free/need pages:1/2 page size:0x40000000
      to reserve more free pages:
      to reserve pages (+orig 1): echo 2 > /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages
      now enough free pages are reserved!

      try to setup hugepage with:
          level 0 - lowmem 0x0, biosmem 0x0, highmem 0x0
          level 1 - lowmem 0x80000000, biosmem 0x0, highmem 0x0
      total_size 0x180000000

      mmap ptr 0x0x7efef33bb000 -> baseaddr 0x0x7eff00000000
      mmap 0x40000000@0x7eff00000000
      touch 1 pages with pagesz 0x40000000
      mmap 0x40000000@0x7eff40000000
      touch 512 pages with pagesz 0x200000
      ...
      [  OK  ] Started Login Service.
      [  OK  ] Started Network Name Resolution.
      [  OK  ] Reached target Network.
               Starting Permit User Sessions...
      [  OK  ] Reached target Host and Network Name Lookups.
      [  OK  ] Started Permit User Sessions.
      [  OK  ] Started Serial Getty on ttyS0.
      [  OK  ] Started Getty on tty1.
      [  OK  ] Started Serial Getty on hvc0.
      [  OK  ] Reached target Login Prompts.
      [  OK  ] Reached target Multi-User System.
      [  OK  ] Reached target Graphical Interface.

      clr-0d449d5327d64aee8a6b8a3484dcd880 login:

#. After you login, these commands and results would show you're running
   in the UOS::

      # uname -r
      4.19.34-45.iot-lts2018
      # ls /dev/acrn*
      ls: cannot access '/dev/acrn*': No such file or directory

   In the UOS there won't be any /dev/acrn* devices.  If you're in the SOS,
   you'd see results such as these::

      # uname -r
      4.19.34-45.iot-lts2018-sos
      # ls /dev/acrn*
      /dev/acrn_hvlog_cur_0   /dev/acrn_hvlog_cur_2  /dev/acrn_trace_0  /dev/acrn_trace_2  /dev/acrn_vhm
      /dev/acrn_hvlog_cur_1   /dev/acrn_hvlog_cur_3  /dev/acrn_trace_1  /dev/acrn_trace_3


.. _manual-setup-guide:

Manual setup ACRN guide
=======================

If you don't need the script to setup ACRN by manual, and then you should follow these steps
after installation of Clear Linux and login system.

#. Clear Linux OS is set to automatically update itself. We recommend that you disable
   this feature to have more control over when the updates happen. Use this command
   to disable the autoupdate feature:

   .. code-block:: none

      $ sudo swupd autoupdate --disable

   .. note::
      The Clear Linux OS installer will automatically check for updates and install the
      latest version available on your system. If you wish to use a specific version
      (such as 28960), you can achieve that after the installation has completed using
      ``sudo swupd verify --fix --picky -m 28960``

#. If you have an older version of Clear Linux OS already installed
   on your hardware, use this command to upgrade Clear Linux OS
   to version 28960 (or newer):

   .. code-block:: none

      $ sudo swupd update -m 28960     # or newer version

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

      .. note::

         ``uart=port@<port address>`` is required if you want to enable the serial console.
         You should run ``dmesg |grep ttyS0`` to get port address from the output, and then
         add the ``uart`` parameter into the ``efibootmgr`` command.

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

   And also here is the example of how to enable a serial console for KBL NUC.

   .. code-block:: none

      $ sudo efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L "ACRN NUC Hypervisor" \
            -u "bootloader=\EFI\org.clearlinux\bootloaderx64.efi uart=port@0x3f8"
            
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
bridge creates ``acrn_br0`` which is the bridge and ``tap0`` as an initial setup. The files can be
found in ``/usr/lib/systemd/network``. No additional setup is needed since systemd-networkd is
automatically enabled after a system restart.

Set up Reference UOS
====================

#. On your platform, download the pre-built reference Clear Linux OS UOS
   image version 28960 (or newer) into your (root) home directory:

   .. code-block:: none

      $ cd ~
      $ mkdir uos
      $ cd uos
      $ curl https://download.clearlinux.org/releases/28960/clear/clear-28960-kvm.img.xz -o uos.img.xz

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

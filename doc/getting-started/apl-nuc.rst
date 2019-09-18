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
  <https://www.amazon.com/Intel-Business-Mini-Technology-BLKNUC7i7DNH1E/dp/B07CCQ8V4R>`__
  (NUC7i7DN), but this is not supported on APL (NUC6CAYH).

.. _connect_serial_port:

Connecting to the serial port
=============================

If you don't need a serial console you can ignore this section. 

Neither the APL or KBL NUCs present an external serial port interface.
However, the KBL NUC does have a serial port header you can
expose with a serial DB9 header cable. You can build this cable yourself,
referring to the `KBL NUC product specification
<https://www.intel.com/content/dam/support/us/en/documents/mini-pcs/nuc-kits/NUC7i7DN_TechProdSpec.pdf>`__
as shown below: 


.. figure:: images/KBL-serial-port-header.png
   :align: center

   KBL serial port header details


.. figure:: images/KBL-serial-port-header-to-RS232-cable.jpg
   :align: center

   KBL `serial port header to RS232 cable
   <https://www.amazon.com/dp/B07BV1W6N8/ref=cm_sw_r_cp_ep_dp_wYm0BbABD5AK6>`_


Or you can `purchase
<https://www.amazon.com/dp/B07BV1W6N8/ref=cm_sw_r_cp_ep_dp_wYm0BbABD5AK6>`_
such a cable.

You'll also need an `RS232 DB9 female to USB cable
<https://www.amazon.com/Adapter-Chipset-CableCreation-Converter-Register/dp/B0769DVQM1>`__,
or an `RS232 DB9 female/female (NULL modem) cross-over cable
<https://www.amazon.com/SF-Cable-Null-Modem-RS232/dp/B006W0I3BA>`__
to connect to your host system.

.. note::
   If you want to use the RS232 DB9 female/female cable, please choose 
   the ``cross-over`` type rather than ``straight-through`` type.

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

We begin by installing Clear Linux* as the development OS on the NUC.
The Clear Linux release includes an ``acrn.efi`` hypervisor application
that will be added to the EFI partition (by the quick setup script or
manually, as described below).

.. note::

   Please refer to the ACRN :ref:`release_notes` for the Clear Linux OS
   version number tested with a specific ACRN release.  Adjust the
   instruction below to reference the appropriate version number of Clear
   Linux OS (we use version 31030 as an example).

#. Download the compressed Clear Linux OS installer image from
   https://download.clearlinux.org/releases/31030/clear/clear-31030-live-server.iso.xz
   and follow the `Clear Linux OS installation guide
   <https://clearlinux.org/documentation/clear-linux/get-started/bare-metal-install-server>`_
   as a starting point for installing Clear Linux OS onto your platform. Follow the recommended
   options for choosing an **Advanced options** installation type, and using the platform's
   storage as the target device for installation (overwriting the
   existing data).

   When setting up Clear Linux on your NUC:

   #.  Launch the Clear Linux OS installer boot menu
   #.  With Clear Linux OS highlighted, select Enter
   #.  Login with your root account, and new password
   #.  Run the installer using the command::

       $ clr-installer

   #.  From the Main Menu, select "Configure Installation Media" and set
       "Destructive Installation" to your desired hard disk.
   #.  Select "Telemetry" to set Tab to highlight your choice.
   #.  Press :kbd:`A` to show the "Advanced options".
   #.  Select "Select additional bundles" to add bundles for
       "desktop-autostart", "editors", "network-basic", "user-basic"
   #.  Select "Manager User" to add an administrative user "clear" and
       password.
   #.  Select "Assign Hostname" to set the hostname as "clr-sos-guest"
   #.  Select "Install".
   #.  Select "Confirm Install" in "Confirm Installtion" window to start installation.

#. After installation is complete, boot into Clear Linux OS, login as
   **clear** (using the password you set earlier).

#. The instructions below provide details for setting
   up the ACRN Hypervisor, Service OS, and Guest OS.  Along with the
   manual step details, We also provide an
   automated script that does all these steps for you, so you can skip these
   manual steps.  See the `quick-setup-guide`_ section below to use the
   automated setup script.

.. _quick-setup-guide:

Use the script to set up ACRN automatically
===========================================

We provide an `acrn_quick_setup.sh script
<https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/doc/getting-started/acrn_quick_setup.sh>`__
in the ACRN GitHub repo to quickly and automatically set up the SOS and UOS
and generate a customized script for launching the UOS.

This script requires the Clear Linux version number you'd like to set up
for the ACRN SOS and UOS.  The version specified must be greater than or
equal to the Clear Linux version currently installed on the NUC.  You
can see your current Clear Linux version with the command::

   $ cat /etc/os-release

.. note:: In the following steps, we're using Clear Linux version 30210.  You should
   specify the Clear Linux version you want to use.

Here are the steps to install Clear Linux on your NUC, set up the SOS
and UOS using the ``acrn_quick_setup.sh`` script, and launch the UOS:

#. Installing Clear Linux and login system

#. Open a terminal

#. Download ``acrn_quick_setup.sh`` script to set up the SOS. (If you don't need a proxy to
   get the script, you can just skip the ``export`` command.)

   .. code-block:: console

      $ export https_proxy=https://myproxy.mycompany.com:port
      $ cd ~
      $ wget https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/doc/getting-started/acrn_quick_setup.sh

      $ sudo sh acrn_quick_setup.sh -s 31030
      Password:
      Upgrading SOS...
      Disable auto update...
      Running systemctl to disable updates
      Clear Linux version 31030 is already installed. Continuing to setup SOS...
      Adding the service-os, kernel-iot-lts2018 and systemd-networkd-autostart bundles...
      Loading required manifests...
      Downloading packs (104.41 MB) for:
       - kernel-iot-lts2018-sos
       - iasimage
       - service-os
       - kernel-iot-lts2018
       - systemd-networkd-autostart
              ...100%
      Finishing packs extraction...
      No extra files need to be downloaded
      Installing bundle(s) files...
              ...100%
      Calling post-update helper scripts
      none
      Successfully installed 3 bundles
      Add /mnt/EFI/acrn folder
      Copy /usr/lib/acrn/acrn.efi to /mnt/EFI/acrn
      Check ACRN efi boot event
      Clean all ACRN efi boot event
      Check linux bootloader event
      Clean all Linux bootloader event
      Add new ACRN efi boot event
      Getting latest Service OS kernel version: org.clearlinux.iot-lts2018-sos.4.19.71-89
      Add default (5 seconds) boot wait time.
      New timeout value is: 5
      Set org.clearlinux.iot-lts2018-sos.4.19.71-89 as default boot kernel.
      Service OS setup done!
      Rebooting Service OS to take effects.
      Rebooting.

   .. note::
      This script is using ``/dev/sda1`` as default EFI System Partition
      ESP). If the ESP is different based on your hardware, you can specify
      it using ``-e`` option.  For example, to set up the SOS on an NVMe
      SSD, you could specify::

         sudo sh acrn_quick_setup.sh -s 31030 -e /dev/nvme0n1p1

   .. note::
      If you don't need to reboot automatically after setting up the SOS, you
      can specify the ``-d`` parameter (don't reboot)

#. After the system reboots, login as the clear user.  You can verify
   the SOS booted successfully by checking the ``dmesg`` log:

   .. code-block:: console

      $ sudo dmesg | grep ACRN
      Password:
      [    0.000000] Hypervisor detected: ACRN
      [    1.252840] ACRNTrace: Initialized acrn trace module with 4 cpu
      [    1.253291] ACRN HVLog: Failed to init last hvlog devs, errno -19
      [    1.253292] ACRN HVLog: Initialized hvlog module with 4 cpu

#. Continue by setting up a Guest OS using the ``acrn_quick_setup.sh``
   script with the ``-u`` option (and the same Clear Linux version
   number):

   .. code-block:: console

      $ sudo sh acrn_quick_setup.sh -u 31030
      Password:
      Upgrading UOS...
      Downloading UOS image: https://download.clearlinux.org/releases/31030/clear/clear-31030-kvm.img.xz
        % Total    % Received % Xferd  Average Speed   Time    Time     Time  Current
                                       Dload  Upload   Total   Spent    Left  Speed
       14  248M   14 35.4M    0     0   851k      0  0:04:57  0:00:42  0:04:15  293k

   After the download is completed, you'll get this output.

   .. code-block:: console

      Unxz UOS image: clear-31030-kvm.img.xz
      Get UOS image: clear-31030-kvm.img
      Upgrade UOS done...
      Now you can run this command to start UOS...
      $ sudo /root/launch_uos_31030.sh

#. Now you can launch the UOS using the customized launch_uos script
   (with sudo):

   .. code-block:: console

      $ sudo /root/launch_uos_31030.sh
      Password:

      cpu1 online=0
      cpu2 online=0
      cpu3 online=0
      passed gvt-g optargs low_gm 64, high_gm 448, fence 8
      SW_LOAD: get ovmf path /usr/share/acrn/bios/OVMF.fd, size 0x200000
      pm by vuart node-index = 0
      logger: name=console, level=4
      logger: name=kmsg, level=3
      logger: name=disk, level=5
      vm_create: vm1
      VHM api version 1.0
      vm_setup_memory: size=0x80000000
      open hugetlbfs file /run/hugepage/acrn/huge_lv1/vm1/D279543825D611E8864ECB7A18B34643
      open hugetlbfs file /run/hugepage/acrn/huge_lv2/vm1/D279543825D611E8864ECB7A18B34643
      level 0 free/need pages:1/1 page size:0x200000
      level 1 free/need pages:2/2 page size:0x40000000

      try to setup hugepage with:
              level 0 - lowmem 0x0, biosmem 0x200000, highmem 0x0
              level 1 - lowmem 0x80000000, biosmem 0x0, highmem 0x0
      total_size 0x180000000

      mmap ptr 0x0x7f792ace5000 -> baseaddr 0x0x7f7940000000
      mmap 0x80000000@0x7f7940000000
      touch 2 pages with pagesz 0x40000000
      mmap 0x200000@0x7f7a3fe00000
      touch 1 pages with pagesz 0x200000
      ...
      [    1.414873] Run /usr/lib/systemd/systemd-bootchart as init process
      [    1.521343] systemd[1]: systemd 242 running in system mode. (+PAM +AUDIT -SELINUX +IMA -APPARMOR -SMACK -SYSVINIT +UTMP +LIBCRYPTSETUP +GCRYPT +GNUTLS +ACL +XZ +LZ4 +SECCOMP +BLKID +ELFUTILS +KMOD -IDN2 -IDN -PCRE2 default-hierarchy=legacy)
      [    1.531173] systemd[1]: Detected virtualization acrn.
      [    1.533287] systemd[1]: Detected architecture x86-64.
      [    1.542775] systemd[1]: Failed to bump fs.file-max, ignoring: Invalid argument
      [    1.681326] systemd[1]: File /usr/lib/systemd/system/systemd-journald.service:12 configures an IP firewall (IPAddressDeny=any), but the local system does not support BPF/cgroup based firewalling.
      [    1.689540] systemd[1]: Proceeding WITHOUT firewalling in effect! (This warning is only shown for the first loaded unit using IP firewalling.)
      [    1.734816] [drm] Cannot find any crtc or sizes
      [    1.860168] systemd[1]: Set up automount Arbitrary Executable File Formats File System Automount Point.
      [    1.870434] systemd[1]: Listening on udev Kernel Socket.
      [    1.875555] systemd[1]: Created slice system-serial\x2dgetty.slice.
      [    1.878446] systemd[1]: Started Dispatch Password Requests to Console Directory Watch.
      [    2.075891] random: systemd-random-: uninitialized urandom read (512 bytes read)
      [    2.239775] [drm] Cannot find any crtc or sizes
      [    3.011537] systemd-journald[133]: Received request to flush runtime journal from PID 1
      [    3.386326] i8042: PNP: PS/2 Controller [PNP0303:KBD,PNP0f13:MOU] at 0x60,0x64 irq 1,12
      [    3.429277] i8042: Warning: Keylock active
      [    3.556872] serio: i8042 KBD port at 0x60,0x64 irq 1
      [    3.610010] serio: i8042 AUX port at 0x60,0x64 irq 12
      [    3.658689] Adding 33788k swap on /dev/vda2.  Priority:-2 extents:1 across:33788k
      [    4.034712] random: dbus-daemon: uninitialized urandom read (12 bytes read)
      [    4.101122] random: tallow: uninitialized urandom read (4 bytes read)
      [    4.119713] random: dbus-daemon: uninitialized urandom read (12 bytes read)
      [    4.223296] virtio_net virtio1 enp0s4: renamed from eth0
      [    4.342645] input: AT Translated Set 2 keyboard as /devices/platform/i8042/serio0/input/input1
      [    4.560662] IPv6: ADDRCONF(NETDEV_UP): enp0s4: link is not ready
      Unhandled ps2 mouse command 0xe1
                                      [    4.725622] IPv6: ADDRCONF(NETDEV_CHANGE): enp0s4: link becomes ready
      [    5.114339] input: PS/2 Generic Mouse as /devices/platform/i8042/serio1/input/input3

      clr-a632ec84744d4e02974fe1891130002e login:

#. Login as root (and specify the new password).  You can verify you're
   running in the UOS by checking the kernel release version or seeing
   if acrn devices are visible:

   .. code-block:: console

      # uname -r
      4.19.71-89.iot-lts2018
      # ls /dev/acrn*
      ls: cannot access '/dev/acrn*': No such file or directory

   In the UOS there won't be any ``/dev/acrn*`` devices.  If you're in the SOS,
   you'd see results such as these:

   .. code-block:: console

      $ uname -r
      4.19.71-89.iot-lts2018-sos
      $ ls /dev/acrn*
      /dev/acrn_hvlog_cur_0   /dev/acrn_hvlog_cur_2  /dev/acrn_trace_0  /dev/acrn_trace_2  /dev/acrn_vhm
      /dev/acrn_hvlog_cur_1   /dev/acrn_hvlog_cur_3  /dev/acrn_trace_1  /dev/acrn_trace_3

With that you've successfully set up Clear Linux at the Service and User
OS and started up a UOS VM.

.. _manual-setup-guide:

Manual setup ACRN guide
=======================

Instead of using the quick setup script, you can also set up ACRN, SOS,
and UOS manually following these steps:

#. After installing Clear Linux on the NUC, login as the **clear** user
   and open a terminal window.
#. Clear Linux OS is set to automatically update itself. We recommend that you disable
   this feature to have more control over when updates happen. Use this command
   to disable the autoupdate feature:

   .. code-block:: none

      $ sudo swupd autoupdate --disable

   .. note::
      The Clear Linux OS installer will automatically check for updates and install the
      latest version available on your system. If you wish to use a specific version
      (such as 31030), you can achieve that after the installation has completed using
      ``sudo swupd repair --picky -V 31030``

#. If you have an older version of Clear Linux OS already installed
   on your hardware, use this command to upgrade Clear Linux OS
   to version 31030 (or newer):

   .. code-block:: none

      $ sudo swupd update -V 31030     # or newer version

#. Use the ``sudo swupd bundle-add`` command and add these Clear Linux OS bundles:

   .. code-block:: none

      $ sudo swupd bundle-add service-os systemd-networkd-autostart

   .. table:: Clear Linux OS bundles
      :widths: auto
      :name: CL-bundles

      +----------------------------+-------------------------------------------+
      | Bundle                     | Description                               |
      +============================+===========================================+
      | service-os                 | Add the acrn hypervisor, acrn             |
      |                            | devicemodel, and Service OS kernel        |
      +----------------------------+-------------------------------------------+
      | systemd-networkd-autostart | Enable systemd-networkd as the default    |
      |                            | network manager                           |
      +----------------------------+-------------------------------------------+


.. _add-acrn-to-efi:

Add the ACRN hypervisor to the EFI Partition
============================================

In order to boot the ACRN SOS on the platform, you'll need to add it to the EFI
partition. Follow these steps:

#. Mount the EFI partition and verify you have the following files:

   .. code-block:: none

      $ sudo ls -1 /boot/EFI/org.clearlinux
      bootloaderx64.efi
      freestanding-00-intel-ucode.cpio
      freestanding-i915-firmware.cpio.xz
      kernel-org.clearlinux.iot-lts2018-sos.4.19.71-89
      kernel-org.clearlinux.native.5.2.14-833
      loaderx64.efi

   .. note::
      On Clear Linux OS, the EFI System Partition (e.g.: ``/dev/sda1``)
      is mounted under ``/boot`` by default
      The Clear Linux project releases updates often, sometimes
      twice a day, so make note of the specific kernel versions
      (*iot-lts2018) listed on your system, as you will need them later.

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

   The ACRN hypervisor (``acrn.efi``) accepts two command-line parameters that
   tweak its behavior:

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


   Here is a more complete example of how to configure the EFI firmware to load the ACRN
   hypervisor and set these parameters.

   .. code-block:: none

      $ sudo efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L "ACRN NUC Hypervisor" \
            -u "bootloader=\EFI\org.clearlinux\bootloaderx64.efi uart=disabled"

   And also here is the example of how to enable a serial console for KBL NUC.

   .. code-block:: none

      $ sudo efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L "ACRN NUC Hypervisor" \
            -u "bootloader=\EFI\org.clearlinux\bootloaderx64.efi uart=port@0x3f8"

#. Add a timeout period for Systemd-Boot to wait, otherwise it will not
   present the boot menu and will always boot the base Clear Linux OS

   .. code-block:: none

      $ sudo clr-boot-manager set-timeout 5
      $ sudo clr-boot-manager update

#. Set the kernel-iot-lts2018 kernel as the default kernel:

   .. code-block:: none

      $ sudo clr-boot-manager list-kernels
      * org.clearlinux.native.5.2.14-833
        org.clearlinux.iot-lts2018-sos.4.19.71-89

   set the default kernel from ``org.clearlinux.native.5.2.14-833`` to
   ``org.clearlinux.iot-lts2018-sos.4.19.71-89``

   .. code-block:: none

      $ sudo clr-boot-manager set-kernel org.clearlinux.iot-lts2018-sos.4.19.71-89
      $ sudo clr-boot-manager list-kernels
        org.clearlinux.native.5.2.14-833
      * org.clearlinux.iot-lts2018-sos.4.19.71-89


#. Reboot and wait until boot menu is displayed, as shown below:

   .. code-block:: console
      :emphasize-lines: 1
      :caption: ACRN Service OS Boot Menu

      Clear Linux OS (Clear-linux-iot-lts2018-sos-4.19.71-89)
      Clear Linux OS (Clear-linux-native.5.2.14-833)
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

   $ sudo dmesg | grep ACRN
   [    0.000000] Hypervisor detected: ACRN
   [    1.253093] ACRNTrace: Initialized acrn trace module with 4 cpu
   [    1.253535] ACRN HVLog: Failed to init last hvlog devs, errno -19
   [    1.253536] ACRN HVLog: Initialized hvlog module with 4 cpu

If you see log information similar to this, the ACRN hypervisor is running properly
and you can start deploying a User OS.  If not, verify the EFI boot options, and SOS
kernel settings are correct (as described above).


ACRN Network Bridge
===================

ACRN bridge has been setup as a part of systemd services for device communication. The default
bridge creates ``acrn_br0`` which is the bridge and ``tap0`` as an initial setup. The files can be
found in ``/usr/lib/systemd/network``. No additional setup is needed since systemd-networkd is
automatically enabled after a system restart.

Set up Reference UOS
====================

#. On your platform, download the pre-built reference Clear Linux OS UOS
   image version 31030 (or newer) into your (root) home directory:

   .. code-block:: none

      $ cd ~
      $ mkdir uos
      $ cd uos
      $ curl https://download.clearlinux.org/releases/31030/clear/clear-31030-kvm.img.xz -o uos.img.xz

   .. note::
      In case you want to use or try out a newer version of Clear Linux OS as the UOS, you can
      download the latest from http://download.clearlinux.org/image/. Make sure to adjust the steps
      described below accordingly (image file name and kernel modules version).

#. Uncompress it:

   .. code-block:: none

      $ unxz uos.img.xz

#. Deploy the UOS kernel modules to UOS virtual disk image (note: you'll need to use
   the same **iot-lts2018** image version number noted in step 1 above):

   .. code-block:: none

      $ sudo losetup -f -P --show uos.img
      $ sudo mount /dev/loop0p3 /mnt
      $ sudo mount /dev/loop0p1 /mnt/boot
      $ sudo swupd bundle-add --path=/mnt kernel-iot-lts2018
      $ uos_kernel_conf=`ls -t /mnt/boot/loader/entries/ | grep Clear-linux-iot-lts2018 | head -n1`
      $ uos_kernel=${uos_kernel_conf%.conf}
      $ sudo echo "default $uos_kernel" > /mnt/boot/loader/loader.conf
      $ sudo umount /mnt/boot
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

      Successful boot

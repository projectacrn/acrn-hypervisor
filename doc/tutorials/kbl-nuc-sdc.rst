.. _kbl-nuc-sdc:

Use SDC Mode on the NUC
#######################

The Intel |reg| NUC is the primary tested platform for ACRN development,
and its setup is described below.

Validated Version
*****************

- Clear Linux version: **32080**
- ACRN-hypervisor tag: **acrn-2012020w02.5.140000p**
- ACRN-Kernel (Service VM kernel): **4.19.94-102.iot-lts2018-sos**

Software Setup
**************

.. _set-up-CL:

Set up a Clear Linux Operating System
=====================================

We begin by installing Clear Linux as the development OS on the NUC.
The Clear Linux release includes an ``acrn.nuc7i7dnb.sdc.efi`` hypervisor application
that will be added to the EFI partition (by the quick setup script or
manually, as described below).

.. note::

   Refer to the ACRN :ref:`release_notes` for the Clear Linux OS
   version number tested with a specific ACRN release.  Adjust the
   instruction below to reference the appropriate version number of Clear
   Linux OS (we use version 32080 as an example).

#. Download the Clear Linux OS installer image from
   https://download.clearlinux.org/releases/31470/clear/clear-31470-live-server.iso
   and follow the `Clear Linux OS Installation Guide
   <https://docs.01.org/clearlinux/latest/get-started/bare-metal-install-server.html>`_
   as a starting point for installing the Clear Linux OS onto your platform.
   Follow the recommended options for choosing an :kbd:`Advanced options`
   installation type, and using the platform's storage as the target device
   for installation (overwriting the existing data).

   When setting up Clear Linux on your NUC:

   #.  Launch the Clear Linux OS installer boot menu.
   #.  With Clear Linux OS highlighted, select :kbd:`Enter`.
   #.  Log in with your root account and new password.
   #.  Run the installer using the following command::

       $ clr-installer

   #.  From the Main menu, select :kbd:`Configure Installation Media` and set
       :kbd:`Destructive Installation` to your desired hard disk.
   #.  Select :kbd:`Telemetry` to set Tab to highlight your choice.
   #.  Press :kbd:`A` to show the :kbd:`Advanced` options.
   #.  Select :kbd:`Select additional bundles` and add bundles for
       **network-basic**, and **user-basic**.
   #.  Select :kbd:`Manager User` to add an administrative user :kbd:`clear` and
       password.
   #.  Select :kbd:`Install`.
   #.  Select :kbd:`Confirm Install` in the :kbd:`Confirm Installation` window to start the installation.

#. After installation is complete, boot into Clear Linux OS, log in as
   :kbd:`clear` (using the password you set earlier).

.. _quick-setup-guide:

Use the script to set up ACRN automatically
===========================================

We provide an `acrn_quick_setup.sh
<https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/doc/getting-started/acrn_quick_setup.sh>`_
script in the ACRN GitHub repo to quickly and automatically set up the Service VM,
User VM and generate a customized script for launching the User VM.

This script requires the Clear Linux version number you'd like to set up
for the ACRN Service VM and User VM. The specified version must be greater than or
equal to the Clear Linux version currently installed on the NUC. You can see
your current Clear Linux version with this command::

   $ cat /etc/os-release

The following instructions use Clear Linux version 31470. Specify the Clear Linux version you want to use.

Follow these steps:

#. Install and log in to Clear Linux.

#. Open a terminal.

#. Download the ``acrn_quick_setup.sh`` script to set up the Service VM.
   (If you don't need a proxy to get the script, skip the ``export`` command.)

   .. code-block:: none

      $ export https_proxy=https://myproxy.mycompany.com:port
      $ cd ~
      $ wget https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/doc/getting-started/acrn_quick_setup.sh
      $ sudo sh acrn_quick_setup.sh -s 32080

#. This output means the script ran successfully.

   .. code-block:: console

      Check ACRN efi boot event
      Clean all ACRN efi boot event
      Check linux bootloader event
      Clean all Linux bootloader event
      Add new ACRN efi boot event, uart is disabled by default.
      + efibootmgr -c -l '\EFI\acrn\acrn.efi' -d /dev/sda -p 1 -L ACRN -u uart=disabled
      Service OS setup done!
      Rebooting Service OS to take effects.
      Rebooting.

   .. note::
      This script is using ``/dev/sda1`` as the default EFI System Partition
      ESP). If the ESP is different based on your hardware, you can specify
      it using the ``-e`` option. For example, to set up the Service VM on an NVMe
      SSD, you could specify:

         ``sudo sh acrn_quick_setup.sh -s 32080 -e /dev/nvme0n1p1``

      If you don't need to reboot automatically after setting up the Service VM, you
      can specify the ``-d`` parameter (don't reboot).

         ``sudo sh acrn_quick_setup.sh -s 32080 -e /dev/nvme0n1p1 -d``

#. After the system reboots, log in as the **clear** user. Verify that the Service VM
   booted successfully by checking the ``dmesg`` log:

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

      $ sudo sh acrn_quick_setup.sh -u 32080
      Password:
      Upgrading User VM...
      Downloading User VM image: https://download.clearlinux.org/releases/32080/clear/clear-32080-kvm.img.xz
        % Total    % Received % Xferd  Average Speed   Time    Time     Time  Current
                                       Dload  Upload   Total   Spent    Left  Speed
       14  248M   14 35.4M    0     0   851k      0  0:04:57  0:00:42  0:04:15  293k

   After the download is complete, you'll get this output.

   .. code-block:: console

      Unxz User VM image: clear-32080-kvm.img.xz
      Get User VM image: clear-32080-kvm.img
      Upgrade User VM done...
      Now you can run this command to start User VM...
      $ sudo /root/launch_uos_32080.sh

#. Launch the User VM using the customized ``launch_uos_32080.sh`` script (with sudo):

   .. code-block:: console

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

#. Log in as root. Specify the new password. Verify that you are running in the User VM
   by checking the kernel release version or seeing if acrn devices are visible:

   .. code-block:: console

      # uname -r
      4.19.94-102.iot-lts2018-sos
      # ls /dev/acrn*
      ls: cannot access '/dev/acrn*': No such file or directory

   The User VM does not have ``/dev/acrn*`` devices.  If you are in the Service VM,
   you will see results such as these:

   .. code-block:: console

      $ uname -r
      4.19.94-102.iot-lts2018-sos
      $ ls /dev/acrn*
      /dev/acrn_hvlog_cur_0   /dev/acrn_hvlog_cur_2  /dev/acrn_trace_0  /dev/acrn_trace_2  /dev/acrn_vhm
      /dev/acrn_hvlog_cur_1   /dev/acrn_hvlog_cur_3  /dev/acrn_trace_1  /dev/acrn_trace_3

You have successfully set up Clear Linux at the Service and User VM and started up a User VM.

.. _manual-setup-guide:

Manually Set Up ACRN
====================

Instead of using the quick setup script, you can also set up ACRN, Service VM,
and User VM manually. Follow these steps:

#. Install Clear Linux on the NUC, log in as the **clear** user,
   and open a terminal window.

#. Disable the auto-update feature. Clear Linux OS is set to automatically update itself.
   We recommend that you disable this feature to have more control over when updates happen. Use this command:

   .. code-block:: none

      $ sudo swupd autoupdate --disable

   .. note::
      When enabled, the Clear Linux OS installer automatically checks for updates and installs the latest version
      available on your system. To use a specific version (such as 32080), enter the following command after the
      installation is complete:

      ``sudo swupd repair --picky -V 32080``

#. If you have an older version of Clear Linux OS already installed
   on your hardware, use this command to upgrade the Clear Linux OS
   to version 32080 (or newer):

   .. code-block:: none

      $ sudo swupd update -V 32080     # or newer version

#. Use the ``sudo swupd bundle-add`` command to add these Clear Linux OS bundles:

   .. code-block:: none

      $ sudo swupd bundle-add service-os systemd-networkd-autostart

   +----------------------------+-------------------------------------------+
   | Bundle                     | Description                               |
   +============================+===========================================+
   | service-os                 | Adds the acrn hypervisor, acrn            |
   |                            | devicemodel, and Service OS kernel        |
   +----------------------------+-------------------------------------------+
   | systemd-networkd-autostart | Enables systemd-networkd as the default   |
   |                            | network manager                           |
   +----------------------------+-------------------------------------------+

.. _add-acrn-to-efi:

Add the ACRN hypervisor to the EFI Partition
============================================

In order to boot the ACRN Service VM on the platform, you must add it to the EFI
partition. Follow these steps:

#. Mount the EFI partition and verify you have the following files:

   .. code-block:: none

      $ sudo ls -1 /boot/EFI/org.clearlinux
      bootloaderx64.efi
      freestanding-00-intel-ucode.cpio
      freestanding-i915-firmware.cpio.xz
      kernel-org.clearlinux.iot-lts2018-sos.4.19.94-102
      kernel-org.clearlinux.native.5.4.11-890
      loaderx64.efi

   .. note::
      On the Clear Linux OS, the EFI System Partition (e.g. ``/dev/sda1``)
      is mounted under ``/boot`` by default. The Clear Linux project releases updates often, sometimes twice a day, so make note of the specific kernel versions (iot-lts2018) listed on your system, as you will need them later.

      The EFI System Partition (ESP) may be different based on your hardware.
      It will typically be something like ``/dev/mmcblk0p1`` on platforms
      that have an on-board eMMC or ``/dev/nvme0n1p1`` if your system has
      a non-volatile storage media attached via a PCI Express (PCIe) bus
      (NVMe).

#. Add the ``acrn.nuc7i7dnb.sdc.efi`` hypervisor application (included in the Clear
   Linux OS release) to the EFI partition. Use these commands:

   .. code-block:: none

      $ sudo mkdir /boot/EFI/acrn
      $ sudo cp /usr/lib/acrn/acrn.nuc7i7dnb.sdc.efi /boot/EFI/acrn/acrn.efi

#. Configure the EFI firmware to boot the ACRN hypervisor by default.

   The ACRN hypervisor (``acrn.efi``) is an EFI executable that's
   loaded directly by the platform EFI firmware. It then loads the
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

   The ACRN hypervisor (``acrn.efi``) accepts two command-line parameters
   that tweak its behavior:

   1. ``bootloader=``: this sets the EFI executable to be loaded once the hypervisor
      is up and running. This is typically the bootloader of the Service OS.
      The default value is to use the Clear Linux OS bootloader, i.e.:
      ``\EFI\org.clearlinux\bootloaderx64.efi``.
   #. ``uart=``: this tells the hypervisor where the serial port (UART) is found or
      whether it should be disabled. There are three forms for this parameter:

      #. ``uart=disabled``: this disables the serial port completely.
      #. ``uart=bdf@<BDF value>``:  this sets the PCI serial port based on its BDF.
         For example, use ``bdf@0:18.1`` for a BDF of 0:18.1 ttyS1.
      #. ``uart=port@<port address>``: this sets the serial port address.

      .. note::

         ``uart=port@<port address>`` is required if you want to enable the serial console.
         Run ``dmesg |grep ttyS0`` to get port address from the output, and then
         add the ``uart`` parameter into the ``efibootmgr`` command.


   Here is a more complete example of how to configure the EFI firmware to load the ACRN
   hypervisor and set these parameters:

   .. code-block:: none

      $ sudo efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L "ACRN NUC Hypervisor" \
            -u "uart=disabled"

   Here is an example of how to enable a serial console for the KBL NUC:

   .. code-block:: none

      $ sudo efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L "ACRN NUC Hypervisor" \
            -u "uart=port@0x3f8"

#. Add a timeout period for the Systemd-Boot to wait; otherwise, it will not
   present the boot menu and will always boot the base Clear Linux OS:

   .. code-block:: none

      $ sudo clr-boot-manager set-timeout 5
      $ sudo clr-boot-manager update

#. Set the kernel-iot-lts2018 kernel as the default kernel:

   .. code-block:: none

      $ sudo clr-boot-manager list-kernels
      * org.clearlinux.native.5.4.11-890
        org.clearlinux.iot-lts2018-sos.4.19.94-102

   Set the default kernel from ``org.clearlinux.native.5.4.11-890`` to
   ``org.clearlinux.iot-lts2018-sos.4.19.94-102``:

   .. code-block:: none

      $ sudo clr-boot-manager set-kernel org.clearlinux.iot-lts2018-sos.4.19.94-102
      $ sudo clr-boot-manager list-kernels
        org.clearlinux.native.5.4.11-890
      * org.clearlinux.iot-lts2018-sos.4.19.94-102

#. Reboot and wait until the boot menu is displayed, as shown below:

   .. code-block:: console
      :emphasize-lines: 1
      :caption: ACRN Service OS Boot Menu

      Clear Linux OS (Clear-linux-iot-lts2018-sos-4.19.94-102)
      Clear Linux OS (Clear-linux-native.5.4.11-890)
      Reboot Into Firmware Interface

#. After booting the ACRN hypervisor, the Service OS launches
   automatically by default, and the Clear Linux OS desktop show with the **clear** user (or you can login remotely with an "ssh" client).
   If there is any issue which makes the GNOME desktop not successfully display,, then the system will go to the shell console.

#. From the ssh client, log in as the **clear** user. Use the password you set previously when you installed the Clear Linux OS.

#. After rebooting the system, check that the ACRN hypervisor is running properly with:

  .. code-block:: none

   $ sudo dmesg | grep ACRN
   [    0.000000] Hypervisor detected: ACRN
   [    1.253093] ACRNTrace: Initialized acrn trace module with 4 cpu
   [    1.253535] ACRN HVLog: Failed to init last hvlog devs, errno -19
   [    1.253536] ACRN HVLog: Initialized hvlog module with 4 cpu

If you see log information similar to this, the ACRN hypervisor is running properly
and you can start deploying a User OS.  If not, verify the EFI boot options, and Service VM
kernel settings are correct (as described above).

ACRN Network Bridge
===================

The ACRN bridge has been set up as a part of systemd services for device
communication. The default bridge creates ``acrn_br0`` which is the bridge and ``tap0`` as an initial setup.
The files can be found in ``/usr/lib/systemd/network``. No additional setup is needed since **systemd-networkd** is
automatically enabled after a system restart.

Set up Reference User VM
========================

#. On your platform, download the pre-built reference Clear Linux OS User VM
   image version 31470 (or newer) into your (root) home directory:

   .. code-block:: none

      $ cd ~
      $ mkdir uos
      $ cd uos
      $ curl https://download.clearlinux.org/releases/32080/clear/clear-32080-kvm.img.xz -o uos.img.xz

   Note that if you want to use or try out a newer version of Clear Linux OS as the User VM, download the
   latest from `http://download.clearlinux.org/image/`.
   Make sure to adjust the steps described below accordingly (image file name and kernel modules version).

#. Uncompress it:

   .. code-block:: none

      $ unxz uos.img.xz

#. Deploy the User VM kernel modules to the User VM virtual disk image (note that you'll need to
   use the same **iot-lts2018** image version number noted in Step 1 above):

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

#. Edit and run the ``launch_uos.sh`` script to launch the User VM.

   A sample `launch_uos.sh
   <https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/devicemodel/samples/nuc/launch_uos.sh>`__
   is included in the Clear Linux OS release, and
   is also available in the ``acrn-hypervisor/devicemodel`` GitHub repo (in the samples
   folder) as shown here:

   .. literalinclude:: ../../../../devicemodel/samples/nuc/launch_uos.sh
      :caption: devicemodel/samples/nuc/launch_uos.sh
      :language: bash

   By default, the script is located in the ``/usr/share/acrn/samples/nuc/``
   directory. You can run it to launch the User OS:

   .. code-block:: none

      $ cd /usr/share/acrn/samples/nuc/
      $ sudo ./launch_uos.sh

#. You have successfully booted the ACRN hypervisor, Service VM, and User VM:

   .. figure:: images/gsg-successful-boot.png
      :align: center

      Successful boot

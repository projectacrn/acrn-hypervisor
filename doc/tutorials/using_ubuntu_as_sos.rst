.. _Ubuntu Service OS:

Running Ubuntu in the Service VM
################################

This document builds on the :ref:`getting_started` series and explains how
to use Ubuntu instead of `Clear Linux OS`_ as the Service VM with the ACRN
hypervisor. (Note that different OSs can be used for the Service and User
VM.) In the following instructions, we will build on material described in
:ref:`kbl-nuc-sdc`.

Install Ubuntu (natively)
*************************

Ubuntu 18.04.1 LTS is used throughout this document; other older versions
such as 16.04 also work.

* Download Ubuntu 18.04 from the `Ubuntu 18.04.1 LTS (Bionic Beaver) page
  <http://releases.ubuntu.com/18.04.1/>`_ and select the `ubuntu-18.04.1-desktop-amd64.iso
  <http://releases.ubuntu.com/18.04.1/ubuntu-18.04.1-desktop-amd64.iso>`_ image.

* Follow Ubuntu's `online instructions <https://tutorials.ubuntu.com/tutorial/tutorial-install-ubuntu-desktop>`_
  to install it on your device.

  .. note::
     Configure your device's proxy settings to have full internet access.

* While not strictly required, enabling SSH gives the user a very useful
  mechanism for accessing the Service VM remotely or when running one or more
  User VM. Follow these steps to enable it on the Ubuntu Service VM:

  .. code-block:: none

     sudo apt-get install openssh-server
     sudo service ssh status
     sudo service ssh start

* If you plan to SSH Ubuntu as root, you must also modify ``/etc/ssh/sshd_config``:

  .. code-block:: none

     PermitRootLogin yes

Install ACRN
************

ACRN components are distributed in source form, so you must download
the source code, build it, and install it on your device.

1. Install the build tools and dependencies.

   Follow the instructions found in :ref:`getting-started-building` to
   install all the build tools and dependencies on your system.

#. Clone the `Project ACRN <https://github.com/projectacrn/acrn-hypervisor>`_
   code repository.

   Enter the following:

   .. code-block:: none

      cd ~
      git clone https://github.com/projectacrn/acrn-hypervisor
      git checkout acrn-2019w47.1-140000p

   .. note::
      We clone the git repository above but it is also possible to download
      the tarball for any specific tag or release from the `Project ACRN
      Github release page <https://github.com/projectacrn/acrn-hypervisor/releases>`_.

#. Build and install ACRN.

   Here is the short version on how to build and install ACRN from source:

   .. code-block:: none

      cd ~/acrn-hypervisor
      make
      sudo make install

   For more details, refer to :ref:`getting-started-building`.

#. Install the hypervisor.

   The ACRN device model and tools are installed as part of the previous
   step. However, ``make install`` does not install the hypervisor (``acrn.efi``) on
   your EFI System Partition (ESP), nor does it configure your EFI firmware
   to boot it automatically. Therefore, follow the steps below to perform
   these operations and complete the ACRN installation.

   #. Add the ACRN hypervisor and Service VM kernel to it (as ``root``):

      .. code-block:: none

         ls /boot/efi/EFI/ubuntu/

      You should see the following output:

      .. code-block:: none

         fw  fwupx64.efi  grub.cfg  grubx64.efi  MokManager.efi  shimx64.efi

   #. Install the hypervisor (``acrn.efi``):

      .. code-block:: none

         sudo mkdir /boot/efi/EFI/acrn/
         sudo cp ~/acrn-hypervisor/build/hypervisor/acrn.efi /boot/efi/EFI/acrn/

   #. Configure the EFI firmware to boot the ACRN hypervisor by default:

      .. code-block:: none

         # For SATA
         sudo efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 \
                -L "ACRN Hypervisor" -u "bootloader=\EFI\ubuntu\grubx64.efi"
         # For NVMe
         sudo efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/nvme0n1 -p 1 \
                -L "ACRN Hypervisor" -u "bootloader=\EFI\ubuntu\grubx64.efi"

   #. Verify that "ACRN Hypervisor" is added and that it will boot first:

      .. code-block:: none

         sudo efibootmgr -v

      You can also verify it by entering the EFI firmware at boot (using :kbd:`F10`).

   #. Change the boot order at any time using ``efibootmgr -o XXX,XXX,XXX``:

     .. code-block:: none

        sudo efibootmgr -o xxx,xxx,xxx

Install the Service VM kernel
*****************************

Download the latest Service VM kernel.

1. The latest Service VM kernel from the latest Clear Linux OS release is
   located here: https://download.clearlinux.org/releases/current/clear/x86_64/os/Packages.  Look for the following ``.rpm`` file:
   ``linux-iot-lts2018-sos-<kernel-version>-<build-version>.x86_64.rpm``.

   While we recommend using the current (latest) Clear Linux OS release, you
   can download a specific Clear Linux release from an area with that
   release number, such as the following:
   https://download.clearlinux.org/releases/31670/clear/x86_64/os/Packages/linux-iot-lts2018-sos-4.19.78-98.x86_64.rpm

#. Download and extract the latest Service VM kernel (this guide uses 31670 as the current example):

   .. code-block:: none

      sudo mkdir ~/sos-kernel-build
      cd ~/sos-kernel-build
      wget https://download.clearlinux.org/releases/31670/clear/x86_64/os/Packages/linux-iot-lts2018-sos-4.19.78-98.x86_64.rpm
      sudo apt-get install rpm2cpio
      rpm2cpio linux-iot-lts2018-sos-4.19.78-98.x86_64.rpm | cpio -idmv

#. Install the Service VM kernel and its drivers (modules):

   .. code-block:: none

      sudo cp -r ~/sos-kernel-build/usr/lib/modules/4.19.78-98.iot-lts2018-sos/ /lib/modules/
      sudo mkdir /boot/acrn/
      sudo cp ~/sos-kernel-build/usr/lib/kernel/org.clearlinux.iot-lts2018-sos.4.19.78-98  /boot/acrn/

#. Configure Grub to load the Service VM kernel:

   * Modify the ``/etc/grub.d/40_custom`` file to create a new Grub entry
     that will boot the Service VM kernel.

     .. code-block:: none

        menuentry 'ACRN Ubuntu Service VM' --id ubuntu-service-vm {
                recordfail
                load_video
                insmod gzio
                insmod part_gpt
                insmod ext2
                linux  /boot/acrn/org.clearlinux.iot-lts2018-sos.4.19.78-98  pci_devices_ignore=(0:18:1) console=tty0 console=ttyS0 root=PARTUUID=<UUID of rootfs partition> rw rootwait ignore_loglevel no_timer_check consoleblank=0 i915.nuclear_pageflip=1 i915.avail_planes_per_pipe=0x01010F i915.domain_plane_owners=0x011111110000 i915.enable_gvt=1 i915.enable_guc=0 hvlog=2M@0x1FE00000
        }

     .. note::
          Adjust this to use your partition UUID (``PARTUUID``) for the
          ``root=`` parameter (or use the device node directly).

          Adjust the kernel name if you used a different RPM file as the
          source of your Service VM kernel.

          The command line for the kernel in ``/etc/grub.d/40_custom``
          should be entered as a single line, not as multiple lines.
          Otherwise, the kernel will fail to boot.

   * Modify the ``/etc/default/grub`` file to make the grub menu visible
     when booting and make it load the Service VM kernel by default.
     Modify the lines shown below:

     .. code-block:: none

        GRUB_DEFAULT=ubuntu-service-vm
        #GRUB_TIMEOUT_STYLE=hidden
        GRUB_TIMEOUT=3

   * Update Grub on your system:

     .. code-block:: none

        sudo update-grub

#. Reboot the system.

   Reboot the system. You should see the Grub menu with the new ACRN ``ubuntu-service-vm``
   entry. Select it and proceed to booting the platform. The system will
   start the Ubuntu Desktop and you can now log in (as before).

   .. note::
       If you don't see the Grub menu after rebooting the system (and you are
       not booting into the ACRN hypervisor), enter the EFI firmware at boot
       (using :kbd:`F10`) and manually select ``ACRN Hypervisor``.

       If you see a black screen on the first-time reboot after installing
       the ACRN Hypervisor, wait a few moments and the Ubuntu desktop will
       display.

   To verify that the hypervisor is effectively running, check ``dmesg``. The
   typical output of a successful installation resembles the following:

   .. code-block:: none

      dmesg | grep ACRN
      [    0.000000] Hypervisor detected: ACRN
      [    0.862942] ACRN HVLog: acrn_hvlog_init

.. _prepare-UOS:

Prepare the User VM
*******************

For the User VM, we are using the same `Clear Linux OS`_ release version as
for the Service VM.

* Download the Clear Linux OS image from `<https://download.clearlinux.org>`_:

  .. code-block:: none

     cd ~
     wget https://download.clearlinux.org/releases/31670/clear/clear-31670-kvm.img.xz
     unxz clear-31670-kvm.img.xz

* Download the "linux-iot-lts2018" kernel:

  .. code-block:: none

     sudo mkdir ~/uos-kernel-build
     cd ~/uos-kernel-build
     wget https://download.clearlinux.org/releases/31670/clear/x86_64/os/Packages/linux-iot-lts2018-sos-4.19.78-98.x86_64.rpm
     rpm2cpio linux-iot-lts2018-4.19.78-98.x86_64.rpm | cpio -idmv

* Update the User VM kernel modules:

  .. code-block:: none

     sudo losetup -f -P --show ~/clear-31670-kvm.img
     sudo mount /dev/loop0p3 /mnt
     sudo cp -r ~/uos-kernel-build/usr/lib/modules/4.19.78-98.iot-lts2018/ /mnt/lib/modules/
     sudo cp -r ~/uos-kernel-build/usr/lib/kernel /lib/modules/
     sudo umount /mnt
     sync

  If you encounter a permission issue, follow these steps:

  .. code-block:: none

     sudo chmod 777 /dev/acrn_vhm

* Add the following package:

  .. code-block:: none

      sudo apt update
      sudo apt install m4 bison flex zlib1g-dev
      cd ~
      wget https://acpica.org/sites/acpica/files/acpica-unix-20191018.tar.gz
      tar zxvf acpica-unix-20191018.tar.gz
      cd acpica-unix-20191018
      make clean && make iasl
      sudo cp ./generate/unix/bin/iasl /usr/sbin/


* Adjust the ``launch_uos.sh`` script:

  You need to adjust the ``/usr/share/acrn/samples/nuc/launch_uos.sh`` script
  to match your installation. Modify the following lines:

  .. code-block:: none

     -s 3,virtio-blk,/root/clear-31670-kvm.img \

  .. note::
      The User VM image can be stored in other directories instead of ``~/``.
      Remember to also modify the image directory in ``launch_uos.sh``.

Start the User VM
*****************

You are now all set to start the User VM:

 .. code-block:: none

   sudo /usr/share/acrn/samples/nuc/launch_uos.sh

**Congratulations**, you are now watching the User VM booting up!


Enable network sharing
**********************

After booting up the Service VM and User VM, network sharing must be enabled
to give network access to the Service VM by enabling the TAP and networking
bridge in the Service VM. The following script example shows how to set
this up (verified in Ubuntu 16.04 and 18.04 as the Service VM).

 .. code-block:: none

    #!/bin/bash
    #setup bridge for uos network
    br=$(brctl show | grep acrn-br0)
    br=${br-:0:6}
    ip tuntap add dev tap0 mode tap

    # if bridge not existed
    if [ "$br"x != "acrn-br0"x ]; then
    #setup bridge for uos network
    brctl addbr acrn-br0
    brctl addif acrn-br0 enp3s0
    ifconfig enp3s0 0
    dhclient acrn-br0
    fi

    # Add TAP device to the bridge
    brctl addif acrn-br0 tap0
    ip link set dev tap0 up

.. note::
   The Service VM network interface is called ``enp3s0`` in the script
   above. Adjust the script if your system uses a different name (e.g.
   ``eno1``).

Enable the USB keyboard and mouse
*********************************

Refer to :ref:`kbl-nuc-sdc` for instructions on enabling the USB keyboard
and mouse for the User VM.


.. _Clear Linux OS: https://clearlinux.org

.. _Ubuntu Service OS:

Using Ubuntu as the Service OS
##############################

This document builds on the :ref:`getting_started`, and explains how to use
Ubuntu instead of using `Clear Linux`_ as the Service OS with the ACRN
hypervisor. (Note that different OSes can be used for the Service and User OS.)
In the following instructions we'll build on material in the
:ref:`getting-started-apl-nuc`.

Install Ubuntu (natively)
*************************

Ubuntu 16.04.4 LTS was used throughout this document, other versions such as
18.04 may work too.

* Download Ubuntu 16.04 from the `Ubuntu 16.04.4 LTS (Xenial Xerus) page
  <https://www.ubuntu.com/download/desktop>`_ and select the `ubuntu-16.04.4-desktop-amd64.iso
  <http://releases.ubuntu.com/16.04/ubuntu-16.04.4-desktop-amd64.iso>`_ image.

* Follow Ubuntu's `online instructions <https://tutorials.ubuntu.com/tutorial/tutorial-install-ubuntu-desktop?_ga=2.114179015.1954550575.1530817291-1278304647.1523530035>`_
  to install it on your device.

Note: Configure your device's proxy settings to have full internet access.

* While not strictly required, enabling SSH gives the user a very useful
  mechanism for accessing the Service OS remotely or when running one or more
  User OS (UOS). Follow these steps to enable it on the Ubuntu SOS:

  .. code-block:: none

     sudo apt-get install openssh-server
     sudo service ssh status
     sudo service ssh start

Install ACRN
************

ACRN components are distributed in source form, so you'll need to download
the source code, build it, and install it on your device.

1. Install the build tools and dependencies

   Follow the instructions found in the :ref:
   https://projectacrn.github.io/latest/getting-started/apl-nuc.html#build-acrn-from-source to install all the build tools 
   and dependencies on your system.

#. Clone the `Project ACRN <https://github.com/projectacrn/acrn-hypervisor>`_
   code repository

   .. code-block:: none

      cd ~
      git clone https://github.com/projectacrn/acrn-hypervisor
      git checkout <known-good-tag/release>

   .. note::
      We clone the git repository below but it is also possible to download the
      tarball for any specific tag or release from the `Project ACRN Github
      release page <https://github.com/projectacrn/acrn-hypervisor/releases>`_

#. Build and install ACRN

   Here is the short version of how to build and install ACRN from source.

   .. code-block:: none

      cd ~/acrn-hypervisor
      make PLATFORM=uefi
      sudo make install

   For more details, please refer to the :ref: https://projectacrn.github.io/latest/getting-started/apl-nuc.html

#. Install the hypervisor

   The ACRN devicemodel and tools were installed as part of the previous step.
   However, ``make install`` does not install the hypervisor (``acrn.efi``) on
   your EFI System Partition (ESP), nor does it configure your EFI firmware to
   boot it automatically. Follow the steps below to perform these operations
   and complete the ACRN installation.

   #. Mount the EFI System Partition (ESP) and add the ACRN hypervisor and
      Service OS kernel to it (as ``root``)

      .. code-block:: none

         sudo umount /boot/efi
         sudo lsblk
         sudo mount /dev/sda1 /mnt
         ls /mnt/EFI/ubuntu

      You should see the following output:

      .. code-block:: none

         fw  fwupx64.efi  grub.cfg  grubx64.efi  MokManager.efi  shimx64.efi

   #. Install the hypervisor (``acrn.efi``)

      .. code-block:: none

         sudo mkdir /mnt/EFI/acrn/
         sudo cp ~/acrn-hypervisor/build/hypervisor/acrn.efi /mnt/EFI/acrn

   #. Configure the EFI firmware to boot the ACRN hypervisor by default

      .. code-block:: none

         sudo efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 \
                -L "ACRN Hypervisor" -u "bootloader=\EFI\ubuntu\grubx64.efi"
         
   # Verify that the "ACRN Hypervisor" and make sure it will be booted first
   .. code-block:: none
      
      sudo efibootmgr -v

   # You can change the boot order at any time using
   .. code-block:: none
   
      efibootmgr -o XXX,XXX,XXX

    .. note::
       By default, the “ACRN Hypervisor” you have just added should be
       the first one to boot. Verify this by using ``efibootmgr -v`` or
       by entering the EFI firmware at boot (using :kbd:`F10`)


Install the Service OS kernel
*****************************

You can download Service OS kernel from clear Linux project: https://download.clearlinux.org/releases

1, Download latest Service OS kernel from latest Clear Linux releasee is here:
`<https://download.clearlinux.org/releases/current/clear/x86_64/os/Packages/>`_
linux-pk414-sos-<``kernel-version``>-<``build-version``>.x86_64.rpm

2, Download Specific Service OS kenrel from specific Clear Linux release:
https://download.clearlinx.org/release/<Clear-linux-release>/clear/x86_64/os/Packages
for example:
Clear-linux-release = 24040
https://download.clearlinux.org/releases/24040/clear/x86_64/os/linux-pk414-sos-4.14.57-69.x86_64.rpm

.. note:: 
       in this guide, we use and recommend people downloading latest release from "current" path

a. Download and extract the latest Service OS kernel(for example: current=24040)

   .. code-block:: none

      mkdir ~/kernel-build
      cd ~/kernel-build
      wget  https://download.clearlinux.org/releases/24040/clear/x86_64/os/Packages/linux-pk414-sos-4.14.57-69.x86_64.rpm
      sudo apt-get install rpm2cpio
      rpm2cpio linux-pk414-sos-4.14.57-69.x86_64.rpm | cpio -idmv

#. Install the SOS kernel and its drivers (modules)

   .. code-block:: none

      sudo cp -r ~/kernel-build/usr/lib/modules/4.14.57-69.pk414-sos/ /lib/modules/
      sudo cp ~/kernel-build/usr/lib/kernel/org.clearlinux.pk414-sos.4.14.57-69 /boot/acrn/

#. Configure Grub to load the Service OS kernel

   * Modify the ``/etc/grub.d/40_custom`` file to create a new Grub entry that
     will boot the SOS kernel.

     .. code-block:: none

        menuentry 'ACRN ubuntu SOS' {
                recordfail
                load_video
                insmod gzio
                insmod part_gpt
                insmod ext2
                linux  /EFI/org.clearlinux/kernel-org.clearlinux.pk414-sos.4.14.57-69 pci_devices_ignore=(0:18:1) maxcpus=1 console=tty0 console=ttyS0 i915.nuclear_pageflip=1 root=PARTUUID=<UUID of rootfs partition> rw rootwait ignore_loglevel no_timer_check consoleblank=0 i915.tsd_init=7 i915.tsd_delay=2000 i915.avail_planes_per_pipe=0x01010F i915.domain_plane_owners=0x011111110000 i915.enable_guc_loading=0 i915.enable_guc_submission=0 i915.enable_preemption=1 i915.context_priority_mode=2 i915.enable_gvt=1 i915.enable_initial_modeset=1 hvlog=2M@0x1FE00000
        }

     .. note::
        You need to adjust this to use your partition UUID (``PARTUUID``) for
        the ``root=`` parameter (or use the device node directly).

     .. note::
        You will also need to adjust the kernel name if you used a different
        RPM file as the source of your Service OS kernel.
      
      .. note::
        The command line for the kernel in /etc/grub.d/40_custom should be all 
        as a single line, not as multiple lines. Otherwise the kernel will fail to boot 

   * Update Grub on your system

     .. code-block:: none

        sudo update-grub

     At this point, you need to modify ``/boot/grub/grub.cfg`` file manually to
     enable the timeout so that the system has an opportunity to show you the
     grub menu. (Without this the grub choice menu won't display.)

     .. code-block:: none

        #set timeout_style=hidden
        set timeout = 10

#. Reboot the system
   
   Reboot system. You should see the Grub menu with the new “ACRN ubuntu SOS”
   entry. Select it and proceed to booting the platform. The system will start
   the Ubuntu Desktop and you can now log in (as before).
   
   .. note::
        if you are not seeing Grub menu after reboot system, you may to into a wrong boot entry
        you may need to select “ACRN Hypervisor” manually by entering the EFI firmware at boot (using :kbd:`F10`)
        
   .. note::
        for the first time reboot after install ACRN, you may see Black Screen after boot up succusfully 
        don't worry about that, this is known issue becaue of fb mode is disabled, wait a moment
        the Ubuntu desktop will be displayed,   
        
   To check if the hypervisor is effectively running, check ``dmesg``. The
   typical output of a successful installation will look like this:

   .. code-block:: none

      dmesg | grep ACRN
      [    0.000000] Hypervisor detected: ACRN
      [    0.862942] ACRN HVLog: acrn_hvlog_init


Prepare the User OS (UOS)
*************************

We are using a User OS based on `Clear Linux`_.
supposing to use same release with Service OS - 24040
https://download.clearlinux.org/releases/24040

* Download the Clear Linux image from `<https://download.clearlinux.org>`_

  .. code-block:: none

     cd ~
     wget https://download.clearlinux.org/releases/24040/clear/clear-24040-kvm.img.xz
     unxz clear-24040-kvm.img.xz

* Download the Production Kenrel (PK) kernel

  .. code-block:: none

     wget https://download.clearlinux.org/releases/24040/clear/x86_64/os/Packages/linux-pk414-standard-4.14.57-69.x86_64.rpm
     rpm2cpio linux-pk414-standard-4.14.57-69.x86_64.rpm | cpio -idmv

* Update the UOS kernel modules

  .. code-block:: none

     sudo losetup -f -P --show /root/clear-24040-kvm.img
     sudo mount /dev/loop0p3 /mnt
     sudo cp -r /root/usr/lib/modules/4.14.57-69.pk414-standard /mnt/lib/modules/
     sudo cp -r /root/usr/lib/kernel /lib/modules/
     sudo umount /mnt
     sync

  If you encounter a permission issue, follow these steps:

  .. code-block:: none

     sudo chmod 777 /dev/acrn_vhm

* One additional package is needed

  .. code-block:: none

     sudo apt-get instal iasl
     sudo cp /usr/bin/iasl /usr/sbin/iasl

* Adjust ``launch_uos.sh``
 
  You need to adjust the ``/usr/share/acrn/samples/nuc/launch_uos.sh`` script
  to match your installation. These are the couple of lines you need to modify:

  .. code-block:: none

     -s 3,virtio-blk,/root/clear-24040-kvm.img
     -k /lib/modules/kernel/org.clearlinux.pk414-standard.4.14.57-69

Start the User OS (UOS)
***********************

You are now all set to start the User OS (UOS)

.. code-block:: none

   sudo /usr/share/acrn/samples/nuc/launch_uos.sh

**Congratulations**, you are now watching the User OS booting up!



Enabling network sharing 
************************

after booting up SOS and UOS, you may need to run some basic test and install addational pakcages
enabling network in UOS is must, enabling the requiring tap and networking bridge in the SOS 
to enable networking in the guest OS is important, you can create a script file to setup.

below is an example for you reference:(verifed in Ubuntu 14.04 and 16.04 as the SOS) 

.. code-block:: none

  #!/bin/bash

  #setup bridge for uos network
  br=$(brctl show | grep acrn-br0)
  br=${br-:0:6}

  ip tuntap add dev acrn_tap0 mode tap
  
  
  taps=$(ifconfig | grep acrn_ | awk '{print $1}')
  
  # if bridge not existed
  if [ "$br"x != "acrn-br0"x ]; then
  #setup bridge for uos network
  brctl addbr acrn-br0
  brctl addif acrn-br0 enp3s0
  ifconfig enp3s0 0
  dhclient acrn-br0

  # add existing tap devices under the bridge
    for tap in $taps; do
      ip tuntap add dev acrn_$tap mode tap
      brctl addif acrn-br0 $tap
      ip link set dev $tap down
      ip link set dev $tap up
    done
  fi
  
  brctl addif acrn-br0 acrn_tap0
  ip link set dev acrn_tap0 up

Enabling USB keyboard and mouse for UOS, please ref: https://projectacrn.github.io/latest/getting-started/apl-nuc.html  
 
 .. _Clear Linux: https://clearlinux.org

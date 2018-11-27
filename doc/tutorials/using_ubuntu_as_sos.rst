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

Ubuntu 18.04.1 LTS was used throughout this document, other older versions such as
16.04 works too.

* Download Ubuntu 18.04 from the `Ubuntu 18.04.1 LTS (Bionic Beaver) page
  <http://releases.ubuntu.com/18.04.1/>`_ and select the `ubuntu-18.04.1-desktop-amd64.iso 
  <http://releases.ubuntu.com/18.04.1/ubuntu-18.04.1-desktop-amd64.iso>`_ image.

* Follow Ubuntu's `online instructions <https://tutorials.ubuntu.com/tutorial/tutorial-install-ubuntu-desktop?_ga=2.114179015.1954550575.1530817291-1278304647.1523530035>`_
  to install it on your device.

.. note::
   Configure your device's proxy settings to have full internet access.

* While not strictly required, enabling SSH gives the user a very useful
  mechanism for accessing the Service OS remotely or when running one or more
  User OS (UOS). Follow these steps to enable it on the Ubuntu SOS:

  .. code-block:: none

     sudo apt-get install openssh-server
     sudo service ssh status
     sudo service ssh start

Install build tools and dependencies
************************************

Install development tools for ARCN development:

* On a Ubuntu development system:

  .. code-block:: none

     sudo apt install gcc
     git
     make
     gnu-efi
     libssl-dev
     libpciaccess-dev
     uuid-dev
     libsystemd-dev
     libevent-dev
     libxml2-dev
     libusb-1.0-0-dev
     python3
     python3-pip
     libblkid-dev
     e2fslibs-dev
     sudo pip3 install kconfiglib
   
.. note::
   You need to use gcc version 7.3.* or higher else you will run into issue `#1396 <https://github.com/projectacrn/acrn-hypervisor/issues/1396>`_. 
   
   Follow these instructions to install the gcc-7 package on Ubuntu 18.04:
   
   
  .. code-block:: none

     sudo add-apt-repository ppa:ubuntu-toolchain-r/test
     sudo apt update
     sudo apt install g++-7 -y
     sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60 \
                     --slave /usr/bin/g++ g++ /usr/bin/g++-7

     
     
Install ACRN
************

ACRN components are distributed in source form, so you'll need to download
the source code, build it, and install it on your device.

1. Install the build tools and dependencies

   Follow the instructions found in the :ref:`getting-started-apl-nuc` to
   install all the build tools and dependencies on your system.

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

   For more details, please refer to the :ref:`getting-started-apl-nuc`.

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
         
   #. Verify that the "ACRN Hypervisor" is added and make sure it will be booted first
      
      .. code-block:: none   
         
         sudo efibootmgr -v

   #. You can change the boot order at any time using ``efibootmgr -o XXX,XXX,XXX``
      
     .. code-block:: none   
         
        sudo efibootmgr -o xxx,xxx,xxx 


   .. note::
      By default, the “ACRN Hypervisor” you have just added should be
      the first one to boot. Verify this by using ``efibootmgr -v`` or
      by entering the EFI firmware at boot (using :kbd:`F10`)

Install the Service OS kernel
*****************************

You can download latest Service OS kernel from
`<https://download.clearlinux.org/releases/current/clear/x86_64/os/Packages/>`_

1. The latest Service OS kernel from the latest Clear Linux release
   from this area:
   https://download.clearlinux.org/releases/current/clear/x86_64/os/Packages.  Look for an
   ``.rpm`` file named ``linux-iot-lts2018-sos-<kernel-version>-<build-version>.x86_64.rpm``.

   While we recommend using the "current" (latest) release of Clear Linux, you can download
   a specific Clear Linux release from an area with that release number, e.g.: 
   https://download.clearlinux.org/releases/26440/clear/x86_64/os/Packages/linux-iot-lts2018-sos-4.19.0-22.x86_64.rpm

#. Download and extract the latest Service OS kernel(this guide is based on 26440 as the current example)

   .. code-block:: none

      mkdir ~/kernel-build
      cd ~/kernel-build
      wget https://download.clearlinux.org/releases/26440/clear/x86_64/os/Packages/linux-iot-lts2018-sos-4.19.0-22.x86_64.rpm
      sudo apt-get install rpm2cpio
      rpm2cpio linux-iot-lts2018-sos-4.19.0-22.x86_64.rpm | cpio -idmv

#. Install the SOS kernel and its drivers (modules)

   .. code-block:: none

      sudo cp -r ~/kernel-build/usr/lib/modules/4.19.0-22.iot-lts2018-sos/ /lib/modules/
      mkdir /boot/acrn/
      sudo cp ~/kernel-build/usr/lib/kernel/org.clearlinux.iot-lts2018-sos.4.19.0-22  /boot/acrn/

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
                linux  /boot/acrn/org.clearlinux.iot-lts2018-sos.4.19.0-22  pci_devices_ignore=(0:18:1)  console=tty0 console=ttyS0 i915.nuclear_pageflip=1 root=PARTUUID=<UUID of rootfs partition> rw rootwait ignore_loglevel no_timer_check consoleblank=0 i915.tsd_init=7 i915.tsd_delay=2000 i915.avail_planes_per_pipe=0x01010F i915.domain_plane_owners=0x011111110000 i915.enable_guc_loading=0 i915.enable_guc_submission=0 i915.enable_preemption=1 i915.context_priority_mode=2 i915.enable_gvt=1 i915.enable_initial_modeset=1 hvlog=2M@0x1FE00000
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

   * Modify the ``/etc/default/grub`` file to make the grub menu visible when booting.
     There are a couple of lines to be modified, as shown below.

     .. code-block:: none

        #GRUB_HIDDEN_TIMEOUT=0
        GRUB_HIDDEN_TIMEOUT_QUIET=false

   * Update Grub on your system

     .. code-block:: none

        sudo update-grub

#. Reboot the system
   
   Reboot system. You should see the Grub menu with the new “ACRN ubuntu SOS”
   entry. Select it and proceed to booting the platform. The system will start
   the Ubuntu Desktop and you can now log in (as before).

   .. note::
       If you don't see the Grub menu after rebooting the system (and you're
       not booting into the ACRN hypervisor), you'll need to enter the
       EFI firmware at boot (using :kbd:`F10`) and manually select ``ACRN Hypervisor``. 
        
   .. note::
       If you see a black screen on the first-time reboot after installing the ACRN Hypervisor, 
       wait a few moments and the Ubuntu desktop will be displayed.
        
   To check if the hypervisor is effectively running, check ``dmesg``. The
   typical output of a successful installation will look like this:

   .. code-block:: none

      dmesg | grep ACRN
      [    0.000000] Hypervisor detected: ACRN
      [    0.862942] ACRN HVLog: acrn_hvlog_init

Prepare the User OS (UOS)
*************************

For the User OS, we are using the same `Clear Linux`_ release version as the Service OS.

* Download the Clear Linux image from `<https://download.clearlinux.org>`_

  .. code-block:: none

     cd ~
     wget https://download.clearlinux.org/releases/26440/clear/clear-26440-kvm.img.xz
     unxz clear-26440-kvm.img.xz

* Download the Production Kernel (PK) kernel

  .. code-block:: none

     wget https://download.clearlinux.org/releases/26440/clear/x86_64/os/Packages/linux-iot-lts2018-4.19.0-22.x86_64.rpm
     rpm2cpio linux-iot-lts2018-4.19.0-22.x86_64.rpm | cpio -idmv

* Update the UOS kernel modules

  .. code-block:: none

     sudo losetup -f -P --show /root/clear-26440-kvm.img
     sudo mount /dev/loop0p3 /mnt
     sudo cp -r /root/usr/lib/modules/4.19.0-22.iot-lts2018/ /mnt/lib/modules/
     sudo cp -r /root/usr/lib/kernel /lib/modules/
     sudo umount /mnt
     sync

  If you encounter a permission issue, follow these steps:

  .. code-block:: none

     sudo chmod 777 /dev/acrn_vhm

* One additional package is needed

  .. code-block:: none

     sudo apt-get install iasl
     sudo cp /usr/bin/iasl /usr/sbin/iasl

* Adjust ``launch_uos.sh``
 
  You need to adjust the ``/usr/share/acrn/samples/nuc/launch_uos.sh`` script
  to match your installation. These are the couple of lines you need to modify:

  .. code-block:: none

     -s 3,virtio-blk,/root/clear-26440-kvm.img
     -k /lib/modules/kernel/default-iot-lts2018

Start the User OS (UOS)
***********************

You are now all set to start the User OS (UOS)

 .. code-block:: none

   sudo /usr/share/acrn/samples/nuc/launch_uos.sh

**Congratulations**, you are now watching the User OS booting up!


Enabling network sharing 
************************

After booting up the SOS and UOS, network sharing must be enabled to give network
access to the UOS by enabling the TAP and networking bridge in the SOS.  The following
script example shows how to set this up (verified in Ubuntu 16.04 and 18.04 as the SOS).


 .. code-block:: none
  
    # !/bin/bash
    # setup bridge for uos network
    br=$(brctl show | grep acrn-br0)
    br=${br-:0:6}
    ip tuntap add dev acrn_tap0 mode tap
    
    taps=$(ifconfig | grep acrn_ | awk '{print $1}')
  
    # if bridge not existed
    if [ "$br"x != "acrn-br0"x ]; then
      # setup bridge for uos network
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

.. note::
   The SOS network interface is called ``enp3s0`` in the script above. You will need
   to adjust the script if your system uses a different name (e.g. ``eno1``).

Enabling USB keyboard and mouse
*******************************

Please refer to :ref:`getting-started-apl-nuc` for enabling the
USB keyboard and mouse for the UOS.

 
.. _Clear Linux: https://clearlinux.org

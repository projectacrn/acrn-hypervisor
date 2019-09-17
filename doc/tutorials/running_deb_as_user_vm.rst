.. _running_deb_as_user_vm:

Running Debian as the User VM
#############################

Prerequisites
*************

This tutorial assumes you have already set up the ACRN Service VM on an
Intel NUC Kit. If you have not, refer to the following instructions:

- Install a `Clear Linux OS <https://docs.01.org/clearlinux/latest/get-started/bare-metal-install-server.html>`_ on your NUC kit.
- Follow the instructions at :ref:`quick-setup-guide` to set up the Service VM automatically on your NUC kit. Follow steps 1 - 4.

We are using Intel Kaby Lake NUC (NUC7i7DNHE) and Debian 10 as the User VM in this tutorial.

Before you start this tutorial, make sure the KVM tools are installed on the
development machine and set **IGD Aperture Size to 512** in the BIOS
settings (refer to :numref:`intel-bios-deb`). Connect two monitors to your
NUC:

.. code-block:: none

   $ sudo apt install qemu-kvm libvirt-clients libvirt-daemon-system bridge-utils virt-manager ovmf

.. figure:: images/debian-uservm-0.png
   :align: center
   :name: intel-bios-deb

   Intel Visual BIOS

We installed these KVM tools on Ubuntu 18.04; refer to the table below for our hardware configurations.

Hardware Configurations
=======================

+--------------------------+----------------------+---------------------------------------------------------------------+
|   Platform (Intel x86)   |   Product/Kit Name   |     Hardware         |   Description                                |
+==========================+======================+======================+=====================================+========+
|       Kaby Lake          |      NUC7i7DNH       |     Processor        | - Intel(R) Core(TM) i7-8650U CPU @ 1.90GHz   |
|                          |                      +----------------------+----------------------------------------------+
|                          |                      |      Graphics        | - UHD Graphics 620                           |
|                          |                      |                      | - Two HDMI 2.0a ports supporting 4K at 60 Hz |
|                          |                      +----------------------+----------------------------------------------+
|                          |                      |    System memory     | - 8GiB SODIMM DDR4 2400 MHz                  |
|                          |                      +----------------------+----------------------------------------------+
|                          |                      | Storage capabilities | - 1TB WDC WD10SPZX-22Z                       |
+--------------------------+----------------------+----------------------+----------------------------------------------+
| PC (development machine) |                      |     Processor        | - Intel(R) Core(TM) i7-2600 CPU @ 3.40GHz    |
|                          |                      +----------------------+----------------------------------------------+
|                          |                      |    System memory     | - 2GiB DIMM DDR3 Synchronous 1333 MHz x 4    |
|                          |                      +----------------------+----------------------------------------------+
|                          |                      | Storage capabilities | - 1TB WDC WD10JPLX-00M                       |
+--------------------------+----------------------+----------------------+----------------------------------------------+



Validated Versions
==================

-  **Clear Linux version:** 30920
-  **ACRN hypervisor tag:** acrn-2019w36.2-140000p
-  **Service VM Kernel version:** 4.19.68-84.iot-lts2018-sos

Build the Debian KVM Image
**************************

This tutorial describes how to build a Debian 10 KVM image. The next few
steps will detail how to use the Debian CD-ROM (ISO) image to install Debian
10 onto a virtual disk.

#. Download the Debian ISO on your development machine:

   .. code-block:: none

      $ mkdir ~/debian10 && cd ~/debian10
      $ wget https://cdimage.debian.org/debian-cd/current/amd64/iso-cd/debian-10.0.0-amd64-netinst.iso

#. Install the Debian ISO via the virt-manager tool:

   .. code-block:: none

      $ sudo virt-manager

#. Verify that you can see the main menu as shown in :numref:`vmmanager-debian` below.

   .. figure:: images/debian-uservm-1.png
      :align: center
      :name: vmmanager-debian

      Virtual Machine Manager

#. Right-click **QEMU/KVM** and select **New**.

   a. Choose **Local install media (ISO image or CDROM)** and then click **Forward**. A **Create a new virtual machine** box displays, as shown in :numref:`newVM-debian` below.

      .. figure:: images/debian-uservm-2.png
         :align: center
         :name: newVM-debian

         Create a New Virtual Machine

    b. Choose **Use ISO image** and click **Browse** - **Browse Local**. Select the ISO which you get from Step 1 above.

    c. Choose the **OS type:** Linux, **Version:** Debian Stretch and then click **Forward**.

    d. Select **Forward** if you do not need to make customized CPU settings.

    e. Choose **Create a disk image for virtual machine**. Set the storage to 20 GB or more if necessary and click **Forward**.

    f. Rename the image if you desire. You must check the **customize configuration before install** option before you finish all stages.

#. Verify that you can see the Overview screen as set up, as shown in :numref:`debian10-setup` below:

    .. figure:: images/debian-uservm-3.png
       :align: center
       :name: debian10-setup

       Debian Setup Overview

#. Complete the Debian installation. Verify that you have set up a vda disk partition, as shown in :numref:`partition-vda` below:

    .. figure:: images/debian-uservm-4.png
       :align: center
       :name: partition-vda

       Virtual Disk (vda) partition

#. Upon installation completion, the KVM image is created in the ``/var/lib/libvirt/images`` folder. Convert the `gcow2` format to `img` **as the root user**:

   .. code-block:: none

      $ cd ~/debian10
      $ qemu-img convert -f qcow2 -O raw /var/lib/libvirt/images/debian10.qcow2 debian10.img

Launch the Debian Image as the User VM
**************************************

Re-use and modify the `launch_win.sh` script in order to launch the new Debian 10 User VM.

.. note:: This tutorial assumes SATA is the default boot drive; replace "/dev/sda1" mentioned below with "/dev/nvme0n1p1" if you are using an NVMe drive.

1. Copy the debian.img to your NUC:

   .. code-block:: none

      # scp ~/debian10/debian10.img user_name@ip_address:~/debian10.img

#. Log in to the ACRN Service VM; generate the launch script and copy OVMF to the image directory:

   .. code-block:: none

      $ cd ~
      $ cp /usr/share/acrn/samples/nuc/launch_win.sh ./launch_debian.sh
      $ sed -i "s/win10-ltsc.img/debian10.img/" launch_debian.sh
      $ cp /usr/share/acrn/bios/OVMF.fd ./

#. Assign USB ports to the Debian VM in order to use the mouse and keyboard before the launch:

   .. code-block:: none

      $ vim launch_debian.sh

      <Add below as the acrn-dm parameter>
      -s 7,xhci,1-2:1-3:1-4:1-5 \

   .. note:: This will assign all USB ports (2 front and 2 rear) to the User VM. If you want to only assign the USB ports at the front, use "-s 7,xhci,1-2:1-3 \" instead. Refer to :ref:`acrn-dm_parameters` for ACRN for more information.

#. Modify acrn.conf and reboot the Service VM to assign the Pipe A monitor to the Debian VM and the Pipe B monitor to the Service VM:

   .. code-block:: none

      $ sudo mount /dev/sda1 /mnt
      $ sudo sed -i "s/0x01010F/0x010101/" /mnt/loader/entries/acrn.conf
      $ sudo sed -i "s/0x011111110000/0x011100001111/" /mnt/loader/entries/acrn.conf
      $ sed -i 3"s/$/ i915.enable_conformance_check=0/" /mnt/loader/entries/acrn.conf
      $ sudo sync && sudo umount /mnt && reboot

#. Copy grubx64.efi to bootx64.efi:

   .. code-block:: none

      $ sudo losetup -f -P --show ~/debian10.img
      $ sudo mount /dev/loop0p1 /mnt
      $ sudo mkdir -p /mnt/EFI/boot
      $ sudo cp /mnt/EFI/debian/grubx64.efi /mnt/EFI/boot/bootx64.efi
      $ sync && sudo umount /mnt

#. Launch the Debian VM afer logging in to the Service VM:

   .. code-block:: none

      $ sudo ./launch_debian.sh

#. View the Debian desktop on the secondary monitor, as shown in :numref:`debian-display2` below:

    .. figure:: images/debian-uservm-5.png
       :align: center
       :name: debian-display1

    .. figure:: images/debian-uservm-6.png
       :align: center
       :name: debian-display2

       The Debian desktop appears on the secondary monitor (bottom image)

Enable the ttyS0 Console on the Debian VM
*****************************************

After the Debian VM reboots, follow the steps below to enable the ttyS0 console so you can make command-line entries directly from it.

1. Log in to the Debian user interface and launch **Terminal** from the Application list.

#. Add "console=ttyS0,115200" to the grub file on the terminal:

   .. code-block:: none

      $ sudo vim /etc/default/grub
      <Add console=ttyS0,115200>
      GRUB_CMDLINE_LINUX="console=ttyS0,115200"
      $ sudo update-grub

#. Add `virtio_console` to `/etc/initramfs-tools/modules`. **Power OFF** the Debian VM after `initramfs` is updated:

   .. code-block:: none

      $ sudo echo "virtio_console" >> /etc/initramfs-tools/modules
      $ sudo update-initramfs -u
      $ sudo poweroff

#. Log in to the Service VM and the modify the launch script to add the `virtio-console` parameter to the Device Model for the Debian VM:

   .. code-block:: none

      $ vim ~/launch_debian.sh
      <add below to the acrn-dm command line>
      -s 9,virtio-console,@stdio:stdio_port \

#. Launch Debian using the modified script. Verify that you see the console output shown in :numref:`console output-debian` below:

    .. figure:: images/debian-uservm-7.png
       :align: center
       :name: console output-debian

       Debian VM console output
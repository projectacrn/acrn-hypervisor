 .. _running_deb_as_serv_vm:

Running Debian as the Service VM
##################################

The `Debian Project <https://www.debian.org/>`_ is an association of individuals who have made common cause to create a `free <https://www.debian.org/intro/free>`_ operating system. The `latest stable Debian release <https://www.debian.org/releases/stable/>`_ is 10.0.

This tutorial describes how to use Debian 10.0 instead of `Clear Linux OS <https://clearlinux.org>`_ as the Service VM with the ACRN hypervisor.

Prerequisites
*************

Use the following instructions to install Debian.

-  Navigate to `Debian 10 iso <https://cdimage.debian.org/debian-cd/current/amd64/iso-cd/>`_. Select and download **debian-10.1.0-amd64-netinst.iso** (scroll down to the bottom of the page).
-  Follow the `Debian installation guide <https://www.debian.org/releases/stable/amd64/index.en.html>`_ to install it on your NUC; we are using an Intel Kaby Lake NUC (NUC7i7DNHE) in this tutorial.
-  :ref:`install-build-tools-dependencies` for ACRN.
-  Update to the latest iASL (required by the ACRN Device Model):

   .. code-block:: bash

      $ sudo apt update
      $ sudo apt install m4 bison flex zlib1g-dev
      $ cd ~
      $ wget https://acpica.org/sites/acpica/files/acpica-unix-20190816.tar.gz
      $ tar zxvf acpica-unix-20190816.tar.gz
      $ cd acpica-unix-20190816
      $ make clean && make iasl
      $ sudo cp ./generate/unix/bin/iasl /usr/sbin/

Validated Versions
******************

-  **Debian version:** 10.0 (buster)
-  **ACRN hypervisor tag:** acrn-2019w35.1-140000p
-  **Debian Service VM Kernel version:** 4.19.68-84.iot-lts2018-sos

Install ACRN on the Debian VM
*****************************

1. Clone the `Project ACRN <https://github.com/projectacrn/acrn-hypervisor>`_ code repository:

   .. code-block:: bash

      $ cd ~
      $ git clone https://github.com/projectacrn/acrn-hypervisor
      $ cd acrn-hypervisor
      $ git checkout acrn-2019w35.1-140000p

#. Build and install ACRN:

   .. code-block:: bash

      $ make BOARD=nuc7i7dnb FIRMWARE=uefi
      $ sudo make install

#. Install the hypervisor.
   The ACRN Device Model and tools were installed as part of a previous step. However, make install does not install the hypervisor (acrn.efi) on your EFI System Partition (ESP), nor does it configure your EFI firmware to boot it automatically. Follow the steps below to perform these operations and complete the ACRN installation. Note that we are using a SATA disk in this section.

   a. Add the ACRN hypervisor (as the root user):

      .. code-block:: bash

         $ sudo mkdir /boot/efi/EFI/acrn/
         $ sudo cp ~/acrn-hypervisor/build/hypervisor/acrn.efi /boot/efi/EFI/acrn/
         $ sudo efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L "ACRN Hypervisor" -u "bootloader=\EFI\debian\grubx64.efi"
         $ sudo efibootmgr -v     # shows output as below
         Timeout: 1 seconds
         BootOrder: 0009,0003,0004,0007,0005,0006,0001,0008,0002,0000
         Boot0000* ACRN  VenHw(99e275e7-75a0-4b37-a2e6-c5385e6c00cb)
         Boot0001* ACRN  VenHw(99e275e7-75a0-4b37-a2e6-c5385e6c00cb)
         Boot0002* debian VenHw(99e275e7-75a0-4b37-a2e6-c5385e6c00cb)
         Boot0003* UEFI : INTEL SSDPEKKW256G8 : PART 0 : OS Bootloader PciRoot(0x0)/Pci(0x1d,0x0)/Pci(0x0,0x0)/NVMe(0x1,00-00-00-00-00-00-00-00)/HD(1,GPT,89d38801-d55b-4bf6-be05-79a5a7b87e66,0x800,0x47000)..BO
         Boot0004* UEFI : INTEL SSDPEKKW256G8 : PART 3 : OS Bootloader PciRoot(0x0)/Pci(0x1d,0x0)/Pci(0x0,0x0)/NVMe(0x1,00-00-00-00-00-00-00-00)/HD(4,GPT,550e1da5-6533-4e64-8d3f-0beadfb20d33,0x1c6da800,0x47000)..BO
         Boot0005* UEFI : LAN : PXE IP4 Intel(R) Ethernet Connection I219-LM  PciRoot(0x0)/Pci(0x1f,0x6)/MAC(54b2030f4b84,0)/IPv4(0.0.0.00.0.0.0,0,0)..BO
         Boot0006* UEFI : LAN : PXE IP6 Intel(R) Ethernet Connection I219-LM  PciRoot(0x0)/Pci(0x1f,0x6)/MAC(54b2030f4b84,0)/IPv6([::]:<->[::]:,0,0)..BO
         Boot0007* UEFI : Built-in EFI Shell     VenMedia(5023b95c-db26-429b-a648-bd47664c8012)..BO
         Boot0008* Linux bootloader VenHw(99e275e7-75a0-4b37-a2e6-c5385e6c00cb)
         Boot0009* ACRN Hypervisor HD(1,GPT,94597852-7166-4216-b0f1-cef5fd1f2349,0x800,0x100000)/File(\EFI\acrn\acrn.efi)b.o.o.t.l.o.a.d.e.r.=.\.E.F.I.\.d.e.b.i.a.n.\.g.r.u.b.x.6.4...e.f.i.

   #. Install the Service VM kernel and reboot:

      .. code-block:: bash

         $ mkdir ~/sos-kernel && cd ~/sos-kernel
         $ wget https://download.clearlinux.org/releases/30930/clear/x86_64/os/Packages/linux-iot-lts2018-sos-4.19.68-84.x86_64.rpm
         $ sudo apt install rpm2cpio
         $ rpm2cpio linux-iot-lts2018-sos-4.19.68-84.x86_64.rpm | cpio -idmv
         $ sudo cp -r ~/sos-kernel/usr/lib/modules/4.19.68-84.iot-lts2018-sos /lib/modules/
         $ sudo mkdir /boot/acrn/
         $ sudo cp ~/sos-kernel/usr/lib/kernel/org.clearlinux.iot-lts2018-sos.4.19.68-84 /boot/acrn/
         $ sudo vi /etc/grub.d/40_custom
         <To add below>
         menuentry 'ACRN Debian Service VM' {
                 recordfail
                 load_video
                 insmod gzio
                 insmod part_gpt
                 insmod ext2

         linux  /boot/acrn/org.clearlinux.iot-lts2018-sos.4.19.68-84 console=tty0 console=ttyS0 root=/dev/sda2 rw rootwait ignore_loglevel no_timer_check consoleblank=0 i915.nuclear_pageflip=1 i915.avail_planes_per_pipe=0x01010F i915.domain_plane_owners=0x011111110000 i915.enable_gvt=1 i915.enable_guc=0 hvlog=2M@0x1FE00000 memmap=2M\$0x1FE00000
         }
         $ sudo vi /etc/default/grub
         <Specify the default grub to the ACRN Debian Service VM entry>
         GRUB_DEFAULT=5
         $ sudo update-grub
         $ sudo reboot

      You should see the Grub menu with the new "ACRN Debian Service VM" entry. Select it and proceed to booting the platform. The system will start the Debian Desktop and you can now log in (as before).

#. Log in to the Debian Service VM and check the ACRN status:

   .. code-block:: bash

      $ dmesg | grep ACRN
      [    0.000000] Hypervisor detected: ACRN
      [    0.981476] ACRNTrace: Initialized acrn trace module with 4 cpu
      [    0.982837] ACRN HVLog: Failed to init last hvlog devs, errno -19
      [    0.983023] ACRN HVLog: Initialized hvlog module with 4 cp

      $ uname -a
      Linux debian 4.19.68-84.iot-lts2018-sos #1 SMP Debian 4.19.37-5+deb10u2 (2019-08-08) x86_64 GNU/Linux

#. Enable the network sharing to give network access to User VM:

   .. code-block:: bash

      $ sudo systemctl enable systemd-networkd
      $ sudo systemctl start systemd-networkd

#. Follow :ref:`prepare-UOS` to start a User VM.
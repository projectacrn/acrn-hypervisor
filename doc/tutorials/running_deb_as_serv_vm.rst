.. _running_deb_as_serv_vm:

Run Debian as the Service VM
############################

The `Debian Project <https://www.debian.org/>`_ is an association of
individuals who have made common cause to create a `free
<https://www.debian.org/intro/free>`_ operating system. The `latest
stable Debian release <https://www.debian.org/releases/stable/>`_ is
10.0.

This tutorial describes how to use Debian 10.0 as the Service VM OS with
the ACRN hypervisor.

Prerequisites
*************

Use the following instructions to install Debian.

- Navigate to `Debian 10 iso
  <https://cdimage.debian.org/debian-cd/current/amd64/iso-cd/>`_.
  Select and download **debian-10.1.0-amd64-netinst.iso** (scroll down to
  the bottom of the page).

  .. note::  These instructions were validated with the
     debian_10.1.0 ISO image. A newer Debian 10 version
     should still work as expected.

- Follow the `Debian installation guide
  <https://www.debian.org/releases/stable/amd64/index.en.html>`_ to
  install it on your board; we are using a Kaby Lake Intel NUC (NUC7i7DNHE)
  in this tutorial.
- :ref:`install-build-tools-dependencies` for ACRN.
- Update to the newer iASL:

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

- **Debian version:** 10.1 (buster)
- **ACRN hypervisor tag:** acrn-2020w40.1-180000p
- **Debian Service VM Kernel version:** release_2.2

Install ACRN on the Debian VM
*****************************

#. Clone the `Project ACRN <https://github.com/projectacrn/acrn-hypervisor>`_ code repository:

   .. code-block:: bash

      $ cd ~
      $ git clone https://github.com/projectacrn/acrn-hypervisor
      $ cd acrn-hypervisor
      $ git checkout acrn-2020w40.1-180000p

#. Build and install ACRN:

   .. code-block:: bash

      $ make all BOARD_FILE=misc/vm_configs/xmls/board-xmls/nuc7i7dnb.xml SCENARIO_FILE=misc/vm_configs/xmls/config-xmls/nuc7i7dnb/industry.xml RELEASE=0
      $ sudo make install
      $ sudo mkdir /boot/acrn/
      $ sudo cp ~/acrn-hypervisor/build/hypervisor/acrn.bin /boot/acrn/

#. Build and Install the Service VM kernel:

   .. code-block:: bash

      $ mkdir ~/sos-kernel && cd ~/sos-kernel
      $ git clone https://github.com/projectacrn/acrn-kernel
      $ cd acrn-kernel
      $ git checkout release_2.2
      $ cp kernel_config_uefi_sos .config
      $ make olddefconfig
      $ make all
      $ sudo make modules_install
      $ sudo cp arch/x86/boot/bzImage /boot/bzImage

#. Update Grub for the Debian Service VM:

   Update the ``/etc/grub.d/40_custom`` file as shown below.

   .. note::
      Enter the command line for the kernel in ``/etc/grub.d/40_custom`` as
      a single line and not as multiple lines. Otherwise, the kernel will
      fail to boot.

   .. code-block:: none

      menuentry "ACRN Multiboot Debian Service VM" --id debian-service-vm {
        recordfail
        load_video
        insmod gzio
        insmod part_gpt
        insmod ext2

        search --no-floppy --fs-uuid --set 9bd58889-add7-410c-bdb7-1fbc2af9b0e1
        echo 'loading ACRN...'
        multiboot2 /boot/acrn/acrn.bin  root=PARTUUID="e515916d-aac4-4439-aaa0-33231a9f4d83"
        module2 /boot/bzImage Linux_bzImage
      }

   .. note::
      Update this to use the UUID (``--set``) and PARTUUID (``root=`` parameter)
      (or use the device node directly) of the root partition (e.g.
      ``/dev/nvme0n1p2``). Hint: use ``sudo blkid <device node>``.

      Update the kernel name if you used a different name as the source
      for your Service VM kernel.

#. Modify the ``/etc/default/grub`` file to make the Grub menu visible when
   booting and make it load the Service VM kernel by default. Modify the
   lines shown below:

   .. code-block:: none

      GRUB_DEFAULT=debian-service-vm
      #GRUB_TIMEOUT_STYLE=hidden
      GRUB_TIMEOUT=5
      GRUB_CMDLINE_LINUX="text"

#. Update Grub on your system:

   .. code-block:: none

      $ sudo update-grub
      $ sudo reboot

#. Log in to the Debian Service VM and check the ACRN status:

   .. code-block:: bash

      $ dmesg | grep ACRN
      [    0.000000] Hypervisor detected: ACRN
      [    0.981476] ACRNTrace: Initialized acrn trace module with 4 cpu
      [    0.982837] ACRN HVLog: Failed to init last hvlog devs, errno -19
      [    0.983023] ACRN HVLog: Initialized hvlog module with 4 cp

Enable Network Sharing to Give Network Access to the User VM
************************************************************

.. code-block:: bash

   $ sudo systemctl enable systemd-networkd
   $ sudo systemctl start systemd-networkd


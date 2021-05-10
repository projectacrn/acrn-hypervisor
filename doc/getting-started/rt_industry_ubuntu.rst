.. _rt_industry_ubuntu_setup:

Getting Started Guide for ACRN Industry Scenario With Ubuntu Service VM
#######################################################################

.. contents::
   :local:
   :depth: 1

Verified Version
****************

- Ubuntu version: **18.04**
- GCC version: **7.5**
- ACRN-hypervisor branch: **release_2.4 (v2.4)**
- ACRN-Kernel (Service VM kernel): **release_2.4 (v2.4)**
- RT kernel for Ubuntu User OS: **4.19/preempt-rt (4.19.72-rt25)**
- HW: Maxtang Intel WHL-U i7-8665U (`AX8665U-A2 <http://www.maxtangpc.com/fanlessembeddedcomputers/140.html>`_)

Prerequisites
*************

- VMX/VT-D are enabled and secure boot is disabled in the BIOS
- Ubuntu 18.04 boot-able USB disk
- Monitors with HDMI interface (DP interface is optional)
- USB keyboard and mouse
- Ethernet cables
- A grub-2.04-7 bootloader with the following patch:

  http://git.savannah.gnu.org/cgit/grub.git/commit/?id=0f3f5b7c13fa9b677a64cf11f20eca0f850a2b20:
  multiboot2: Set min address for mbi allocation to 0x1000

.. rst-class:: numbered-step

Hardware Connection
*******************

Connect the WHL Maxtang with the appropriate external devices.

#. Connect the WHL Maxtang board to a monitor via an HDMI cable.
#. Connect the mouse, keyboard, Ethernet cable, and power supply cable to
   the WHL Maxtang board.
#. Insert the Ubuntu 18.04 USB boot disk into the USB port.

   .. figure:: images/rt-ind-ubun-hw-1.png

   .. figure:: images/rt-ind-ubun-hw-2.png

.. rst-class:: numbered-step


.. _install-ubuntu-rtvm-sata:

Install the Ubuntu User VM (RTVM) on the SATA Disk
**************************************************

.. note:: The WHL Maxtang machine contains both an NVMe and SATA disk.
   Before you install the Ubuntu User VM on the SATA disk, either
   remove the NVMe disk or delete its blocks.

#. Insert the Ubuntu USB boot disk into the WHL Maxtang machine.
#. Power on the machine, then press F11 to select the USB disk as the boot
   device. Select **UEFI: SanDisk** to boot using **UEFI**. Note that the
   label depends on the brand/make of the USB drive.
#. Install the Ubuntu OS.
#. Select **Something else** to create the partition.

   .. figure:: images/native-ubuntu-on-SATA-1.png

#. Configure the ``/dev/sda`` partition. Refer to the diagram below:

   .. figure:: images/native-ubuntu-on-SATA-2.png

   a. Select the ``/dev/sda`` partition, not ``/dev/nvme0p1``.
   b. Select ``/dev/sda`` **ATA KINGSTON RBUSNS4** as the device for the
      bootloader installation. Note that the label depends on the SATA disk used.

#. Complete the Ubuntu installation on ``/dev/sda``.

This Ubuntu installation will be modified later (see `Build and Install the RT kernel for the Ubuntu User VM`_)
to turn it into a real-time User VM (RTVM).

.. rst-class:: numbered-step

.. _install-ubuntu-Service VM-NVMe:

Install the Ubuntu Service VM on the NVMe Disk
**********************************************

.. note:: Before you install the Ubuntu Service VM on the NVMe disk, either
   remove the SATA disk or disable it in the BIOS. Disable it by going to:
   **Chipset** → **PCH-IO Configuration** -> **SATA and RST Configuration** -> **SATA Controller [Disabled]**

#. Insert the Ubuntu USB boot disk into the WHL Maxtang machine.
#. Power on the machine, then press F11 to select the USB disk as the boot
   device. Select **UEFI: SanDisk** to boot using **UEFI**. Note that the
   label depends on the brand/make of the USB drive.
#. Install the Ubuntu OS.
#. Select **Something else** to create the partition.

   .. figure:: images/native-ubuntu-on-NVME-1.png

#. Configure the ``/dev/nvme0n1`` partition. Refer to the diagram below:

   .. figure:: images/native-ubuntu-on-NVME-2.png

   a. Select the ``/dev/nvme0n1`` partition, not ``/dev/sda``.
   b. Select ``/dev/nvme0n1`` **FORESEE 256GB SSD** as the device for the
      bootloader installation. Note that the label depends on the NVMe disk used.

#. Complete the Ubuntu installation and reboot the system.

   .. note:: Set ``acrn`` as the username for the Ubuntu Service VM.


.. rst-class:: numbered-step

.. _build-and-install-acrn-on-ubuntu:

Build and Install ACRN on Ubuntu
********************************

Pre-Steps
=========

#. Set the network configuration, proxy, etc.
#. Update Ubuntu:

   .. code-block:: none

      $ sudo -E apt update

#. Create a work folder:

   .. code-block:: none

      $ mkdir /home/acrn/work

Build the ACRN Hypervisor on Ubuntu
===================================

#. Install the necessary libraries:

   .. code-block:: none

      $ sudo -E apt install gcc \
        git \
        make \
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
        e2fslibs-dev \
        pkg-config \
        libnuma-dev \
        liblz4-tool \
        flex \
        bison \
        xsltproc

      $ sudo pip3 install lxml xmlschema

#. Starting with the ACRN v2.2 release, we use the ``iasl`` tool to
   compile an offline ACPI binary for pre-launched VMs while building ACRN,
   so we need to install the ``iasl`` tool in the ACRN build environment.

   Follow these steps to install ``iasl`` (and its dependencies) and
   then update the ``iasl`` binary with a newer version not available
   in Ubuntu 18.04:

   .. code-block:: none

      $ cd /home/acrn/work
      $ wget https://acpica.org/sites/acpica/files/acpica-unix-20210105.tar.gz
      $ tar zxvf acpica-unix-20210105.tar.gz
      $ cd acpica-unix-20210105
      $ make clean && make iasl
      $ sudo cp ./generate/unix/bin/iasl /usr/sbin/

#. Get the ACRN source code:

   .. code-block:: none

      $ cd /home/acrn/work
      $ git clone https://github.com/projectacrn/acrn-hypervisor
      $ cd acrn-hypervisor

#. Switch to the v2.4 version:

   .. code-block:: none

      $ git checkout v2.4

#. Build ACRN:

   .. code-block:: none

      $ make BOARD=whl-ipc-i7 SCENARIO=industry
      $ sudo make install
      $ sudo mkdir -p /boot/acrn
      $ sudo cp build/hypervisor/acrn.bin /boot/acrn/

.. _build-and-install-ACRN-kernel:

Build and Install the ACRN Kernel
=================================

#. Build the Service VM kernel from the ACRN repo:

   .. code-block:: none

      $ cd /home/acrn/work/
      $ git clone https://github.com/projectacrn/acrn-kernel
      $ cd acrn-kernel

#. Switch to the 5.4 kernel:

   .. code-block:: none

      $ git checkout v2.4
      $ cp kernel_config_uefi_sos .config
      $ make olddefconfig
      $ make all

Install the Service VM Kernel and Modules
=========================================

.. code-block:: none

   $ sudo make modules_install
   $ sudo cp arch/x86/boot/bzImage /boot/bzImage

Update Grub for the Ubuntu Service VM
=====================================

#. Update the ``/etc/grub.d/40_custom`` file as shown below.

   .. note::
      Enter the command line for the kernel in ``/etc/grub.d/40_custom`` as
      a single line and not as multiple lines. Otherwise, the kernel will
      fail to boot.

   .. code-block:: none

      menuentry "ACRN Multiboot Ubuntu Service VM" --id ubuntu-service-vm {
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

      Add the ``menuentry`` at the bottom of :file:`40_custom`, keep the
      ``exec tail`` line at the top intact.

#. Modify the ``/etc/default/grub`` file to make the Grub menu visible when
   booting and make it load the Service VM kernel by default. Modify the
   lines shown below:

   .. code-block:: none

      GRUB_DEFAULT=ubuntu-service-vm
      #GRUB_TIMEOUT_STYLE=hidden
      GRUB_TIMEOUT=5
      GRUB_CMDLINE_LINUX="text"

#. Update Grub on your system:

   .. code-block:: none

      $ sudo update-grub

Enable Network Sharing for the User VM
======================================

In the Ubuntu Service VM, enable network sharing for the User VM:

.. code-block:: none

   $ sudo systemctl enable systemd-networkd
   $ sudo systemctl start systemd-networkd


Reboot the System
=================

Reboot the system. You should see the Grub menu with the new **ACRN
ubuntu-service-vm** entry. Select it and proceed to booting the platform. The
system will start Ubuntu and you can now log in (as before).

To verify that the hypervisor is effectively running, check ``dmesg``. The
typical output of a successful installation resembles the following:

.. code-block:: none

   $ dmesg | grep ACRN
   [    0.000000] Hypervisor detected: ACRN
   [    0.862942] ACRN HVLog: acrn_hvlog_init


Additional Settings in the Service VM
=====================================

BIOS Settings of GVT-d for WaaG
-------------------------------

.. note::
   Skip this step if you are using a Kaby Lake (KBL) Intel NUC.

Go to **Chipset** -> **System Agent (SA) Configuration** -> **Graphics
Configuration** and make the following settings:

Set **DVMT Pre-Allocated** to **64MB**:

.. figure:: images/DVMT-reallocated-64mb.png

Set **PM Support** to **Enabled**:

.. figure:: images/PM-support-enabled.png

Use OVMF to Launch the User VM
------------------------------

The User VM will be launched by OVMF, so copy it to the specific folder:

.. code-block:: none

   $ sudo mkdir -p /usr/share/acrn/bios
   $ sudo cp /home/acrn/work/acrn-hypervisor/devicemodel/bios/OVMF.fd  /usr/share/acrn/bios

Build and Install the RT Kernel for the Ubuntu User VM
------------------------------------------------------

Follow these instructions to build the RT kernel.

#. Clone the RT kernel source code:

   .. note::
      This guide assumes you are doing this within the Service VM. This
      **acrn-kernel** repository was already cloned under ``/home/acrn/work``
      earlier on so you can just ``cd`` into it and perform the ``git checkout``
      directly.

   .. code-block:: none

      $ git clone https://github.com/projectacrn/acrn-kernel
      $ cd acrn-kernel
      $ git checkout 4.19/preempt-rt
      $ make mrproper

   .. note::
      The ``make mrproper`` is to make sure there is no ``.config`` file
      left from any previous build (e.g. the one for the Service VM kernel).

#. Build the kernel:

   .. code-block:: none

      $ cp x86-64_defconfig .config
      $ make olddefconfig
      $ make targz-pkg

#. Copy the kernel and modules:

   .. code-block:: none

      $ sudo mount /dev/sda2 /mnt
      $ sudo cp arch/x86/boot/bzImage /mnt/boot/
      $ sudo tar -zxvf linux-4.19.72-rt25-x86.tar.gz -C /mnt/lib/modules/
      $ sudo cp -r /mnt/lib/modules/lib/modules/4.19.72-rt25 /mnt/lib/modules/
      $ sudo cd ~ && sudo umount /mnt && sync

.. rst-class:: numbered-step

Launch the RTVM
***************

Grub in the Ubuntu User VM (RTVM) needs to be configured to use the new RT
kernel that was just built and installed on the rootfs. Follow these steps to
perform this operation.

Update the Grub File
====================

#. Reboot into the Ubuntu User VM located on the SATA drive and log on.

#. Update the ``/etc/grub.d/40_custom`` file as shown below.

   .. note::
      Enter the command line for the kernel in ``/etc/grub.d/40_custom`` as
      a single line and not as multiple lines. Otherwise, the kernel will
      fail to boot.

   .. code-block:: none

      menuentry "ACRN Ubuntu User VM" --id ubuntu-user-vm {
        load_video
        insmod gzio
        insmod part_gpt
        insmod ext2

        search --no-floppy --fs-uuid --set b2ae4879-c0b6-4144-9d28-d916b578f2eb
        echo 'loading ACRN...'

        linux  /boot/bzImage root=PARTUUID=<UUID of rootfs partition> rw rootwait nohpet console=hvc0 console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M consoleblank=0 clocksource=tsc tsc=reliable x2apic_phys processor.max_cstate=0 intel_idle.max_cstate=0 intel_pstate=disable mce=ignore_ce audit=0 isolcpus=nohz,domain,1 nohz_full=1 rcu_nocbs=1 nosoftlockup idle=poll irqaffinity=0
      }

   .. note::
      Update this to use the UUID (``--set``) and PARTUUID (``root=`` parameter)
      (or use the device node directly) of the root partition (e.g. ``/dev/sda2).
      Hint: use ``sudo blkid /dev/sda*``.

      Update the kernel name if you used a different name as the source
      for your Service VM kernel.

      Add the ``menuentry`` at the bottom of :file:`40_custom`, keep the
      ``exec tail`` line at the top intact.

#. Modify the ``/etc/default/grub`` file to make the grub menu visible when
   booting and make it load the RT kernel by default. Modify the
   lines shown below:

   .. code-block:: none

      GRUB_DEFAULT=ubuntu-user-vm
      #GRUB_TIMEOUT_STYLE=hidden
      GRUB_TIMEOUT=5

#. Update Grub on your system:

   .. code-block:: none

      $ sudo update-grub

#. Reboot into the Ubuntu Service VM

Launch the RTVM
===============

  .. code-block:: none

     $ sudo cp /home/acrn/work/acrn-hyperviso/misc/config_tools/data/sample_launch_scripts/nuc/launch_hard_rt_vm.sh  /usr/share/acrn/
     $ sudo /usr/share/acrn/launch_hard_rt_vm.sh

.. note::
   If using a KBL NUC, the script must be adapted to match the BDF on the actual HW platform

Recommended BIOS Settings for RTVM
----------------------------------

.. csv-table::
   :widths: 15, 30, 10

   "Hyper-threading", "Intel Advanced Menu -> CPU Configuration", "Disabled"
   "Intel VMX", "Intel Advanced Menu -> CPU Configuration", "Enable"
   "Speed Step", "Intel Advanced Menu -> Power & Performance -> CPU - Power Management Control", "Disabled"
   "Speed Shift", "Intel Advanced Menu -> Power & Performance -> CPU - Power Management Control", "Disabled"
   "C States", "Intel Advanced Menu -> Power & Performance -> CPU - Power Management Control", "Disabled"
   "RC6", "Intel Advanced Menu -> Power & Performance -> GT - Power Management", "Disabled"
   "GT freq", "Intel Advanced Menu -> Power & Performance -> GT - Power Management", "Lowest"
   "SA GV", "Intel Advanced Menu -> Memory Configuration", "Fixed High"
   "VT-d", "Intel Advanced Menu -> System Agent Configuration", "Enable"
   "Gfx Low Power Mode", "Intel Advanced Menu -> System Agent Configuration -> Graphics Configuration", "Disabled"
   "DMI spine clock gating", "Intel Advanced Menu -> System Agent Configuration -> DMI/OPI Configuration", "Disabled"
   "PCH Cross Throttling", "Intel Advanced Menu -> PCH-IO Configuration", "Disabled"
   "Legacy IO Low Latency", "Intel Advanced Menu -> PCH-IO Configuration -> PCI Express Configuration", "Enabled"
   "PCI Express Clock Gating", "Intel Advanced Menu -> PCH-IO Configuration -> PCI Express Configuration", "Disabled"
   "Delay Enable DMI ASPM", "Intel Advanced Menu -> PCH-IO Configuration -> PCI Express Configuration", "Disabled"
   "DMI Link ASPM", "Intel Advanced Menu -> PCH-IO Configuration -> PCI Express Configuration", "Disabled"
   "Aggressive LPM Support", "Intel Advanced Menu -> PCH-IO Configuration -> SATA And RST Configuration", "Disabled"
   "USB Periodic SMI", "Intel Advanced Menu -> LEGACY USB Configuration", "Disabled"
   "ACPI S3 Support", "Intel Advanced Menu -> ACPI Settings", "Disabled"
   "Native ASPM", "Intel Advanced Menu -> ACPI Settings", "Disabled"

.. note:: BIOS settings depend on the platform and BIOS version; some may
   not be applicable.

Recommended Kernel Cmdline for RTVM
-----------------------------------

.. code-block:: none

   root=PARTUUID=<UUID of rootfs partition> rw rootwait nohpet console=hvc0 console=ttyS0 \
   no_timer_check ignore_loglevel log_buf_len=16M consoleblank=0 \
   clocksource=tsc tsc=reliable x2apic_phys processor.max_cstate=0 \
   intel_idle.max_cstate=0 intel_pstate=disable mce=ignore_ce audit=0 \
   isolcpus=nohz,domain,1 nohz_full=1 rcu_nocbs=1 nosoftlockup idle=poll \
   irqaffinity=0


Configure RDT
-------------

In addition to setting the CAT configuration via HV commands, we allow
developers to add CAT configurations to the VM config and configure
automatically at the time of RTVM creation. Refer to :ref:`rdt_configuration`
for details on RDT configuration and :ref:`hv_rdt` for details on RDT
high-level design.

Set Up the Core Allocation for the RTVM
---------------------------------------

In our recommended configuration, two cores are allocated to the RTVM:
core 0 for housekeeping and core 1 for RT tasks. In order to achieve
this, follow the below steps to allocate all housekeeping tasks to core 0:

#. Prepare the RTVM launch script

   Follow the `Passthrough a hard disk to RTVM`_ section to make adjustments to
   the ``/usr/share/acrn/launch_hard_rt_vm.sh`` launch script.

#. Launch the RTVM:

   .. code-block:: none

      $ sudo /usr/share/acrn/launch_hard_rt_vm.sh

#. Log in to the RTVM as root and run the script as below:

   .. code-block:: none

      #!/bin/bash
      # Copyright (C) 2019 Intel Corporation.
      # SPDX-License-Identifier: BSD-3-Clause
      # Move all IRQs to core 0.
      for i in `cat /proc/interrupts | grep '^ *[0-9]*[0-9]:' | awk {'print $1'} | sed 's/:$//' `;
      do
          echo setting $i to affine for core zero
          echo 1 > /proc/irq/$i/smp_affinity
      done

      # Move all rcu tasks to core 0.
      for i in `pgrep rcu`; do taskset -pc 0 $i; done

      # Change real-time attribute of all rcu tasks to SCHED_OTHER and priority 0
      for i in `pgrep rcu`; do chrt -v -o -p 0 $i; done

      # Change real-time attribute of all tasks on core 1 to SCHED_OTHER and priority 0
      for i in `pgrep /1`; do chrt -v -o -p 0 $i; done

      # Change real-time attribute of all tasks to SCHED_OTHER and priority 0
      for i in `ps -A -o pid`; do chrt -v -o -p 0 $i; done

      echo disabling timer migration
      echo 0 > /proc/sys/kernel/timer_migration

   .. note:: Ignore the error messages that might appear while the script is
      running.

Run Cyclictest
--------------

#. Refer to the :ref:`troubleshooting section <enabling the network on the RTVM>`
   below that discusses how to enable the network connection for RTVM.

#. Launch the RTVM and log in as root.

#. Install the ``rt-tests`` tool:

   .. code-block:: none

      # apt install rt-tests

#. Use the following command to start cyclictest:

   .. code-block:: none

      # cyclictest -a 1 -p 80 -m -N -D 1h -q -H 30000 --histfile=test.log


   Parameter descriptions:

    :-a 1:                           to bind the RT task to core 1
    :-p 80:                          to set the priority of the highest prio thread
    :-m:                             lock current and future memory allocations
    :-N:                             print results in ns instead of us (default us)
    :-D 1h:                          to run for 1 hour, you can change it to other values
    :-q:                             quiet mode; print a summary only on exit
    :-H 30000 --histfile=test.log:   dump the latency histogram to a local file

.. rst-class:: numbered-step

Launch the Windows VM
*********************

#. Follow this :ref:`guide <using_windows_as_uos>` to prepare the Windows
   image file and then reboot with a new ``acrngt.conf``.

#. Modify the ``launch_uos_id1.sh`` script as follows and then launch
   the Windows VM as one of the post-launched standard VMs:

   .. code-block:: none
      :emphasize-lines: 2

      acrn-dm -A -m $mem_size -s 0:0,hostbridge -s 1:0,lpc -l com1,stdio \
         -s 2,passthru,0/2/0,gpu \
         -s 3,virtio-blk,./win10-ltsc.img \
         -s 4,virtio-net,tap0 \
         --ovmf /usr/share/acrn/bios/OVMF.fd \
         --windows \
         $vm_name

Troubleshooting
***************

.. _enabling the network on the RTVM:

Enabling the Network on the RTVM
================================

If you need to access the internet, you must add the following command line
to the ``launch_hard_rt_vm.sh`` script before launching it:

.. code-block:: none
   :emphasize-lines: 8

   acrn-dm -A -m $mem_size -s 0:0,hostbridge \
      --lapic_pt \
      --rtvm \
      --virtio_poll 1000000 \
      -U 495ae2e5-2603-4d64-af76-d4bc5a8ec0e5 \
      -s 2,passthru,02/0/0 \
      -s 3,virtio-console,@stdio:stdio_port \
      -s 8,virtio-net,tap0 \
      --ovmf /usr/share/acrn/bios/OVMF.fd \
      hard_rtvm

.. _passthru to rtvm:

Passthrough a Hard Disk to RTVM
===============================

#. Use the ``lspci`` command to ensure that the correct SATA device IDs will
   be used for the passthrough before launching the script:

   .. code-block:: none

      # lspci -nn | grep -i sata
      00:17.0 SATA controller [0106]: Intel Corporation Cannon Point-LP SATA Controller [AHCI Mode] [8086:9dd3] (rev 30)

#. Modify the script to use the correct SATA device IDs and bus number:

   .. code-block:: none

      # vim /usr/share/acrn/launch_hard_rt_vm.sh

      passthru_vpid=(
      ["eth"]="8086 156f"
      ["sata"]="8086 9dd3"
      ["nvme"]="8086 f1a6"
      )
      passthru_bdf=(
      ["eth"]="0000:00:1f.6"
      ["sata"]="0000:00:17.0"
      ["nvme"]="0000:02:00.0"
      )

      # SATA pass-through
      echo ${passthru_vpid["sata"]} > /sys/bus/pci/drivers/pci-stub/new_id
      echo ${passthru_bdf["sata"]} > /sys/bus/pci/devices/${passthru_bdf["sata"]}/driver/unbind
      echo ${passthru_bdf["sata"]} > /sys/bus/pci/drivers/pci-stub/bind

      # NVME pass-through
      #echo ${passthru_vpid["nvme"]} > /sys/bus/pci/drivers/pci-stub/new_id
      #echo ${passthru_bdf["nvme"]} > /sys/bus/pci/devices/${passthru_bdf["nvme"]}/driver/unbind
      #echo ${passthru_bdf["nvme"]} > /sys/bus/pci/drivers/pci-stub/bind

   .. code-block:: none
      :emphasize-lines: 5

         --lapic_pt \
         --rtvm \
         --virtio_poll 1000000 \
         -U 495ae2e5-2603-4d64-af76-d4bc5a8ec0e5 \
         -s 2,passthru,00/17/0 \
         -s 3,virtio-console,@stdio:stdio_port \
         -s 8,virtio-net,tap0 \
         --ovmf /usr/share/acrn/bios/OVMF.fd \
         hard_rtvm

#. Upon deployment completion, launch the RTVM directly onto your WHL
   Intel NUC:

   .. code-block:: none

      $ sudo /usr/share/acrn/launch_hard_rt_vm.sh

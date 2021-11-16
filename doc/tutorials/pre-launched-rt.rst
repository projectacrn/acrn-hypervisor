.. _pre_launched_rt:

Pre-Launched Preempt-RT Linux Mode in ACRN
##########################################

The Pre-Launched Preempt-RT Linux Mode of ACRN, abbreviated as
Pre-Launched RT mode, is an ACRN configuration scenario. Pre-Launched RT
mode enables you to create a pre-launched real-time VM (RTVM) running
Preempt-RT Linux (VM0) and a Service VM (VM1). Their resources are partitioned
from those on the physical platform.

.. figure:: images/pre_launched_rt.png
   :align: center

Prerequisites
*************

Because the pre-launched RTVM and Service VM are physically isolated
from each other, they must have their own devices to run a common OS,
such as Linux. Also, the platform must support booting ACRN with
multiple kernel images. So, your platform must have:

- Two hard disk drives, one for the pre-launched RTVM and one for the Service
  VM
- Two network devices
- GRUB multiboot support

Example of Pre-Launched RT
**************************

Take the Whiskey Lake WHL-IPC-I5 board (as described in :ref:`hardware`) for
example. This platform can connect both an NVMe and a SATA drive and has
two Ethernet ports. We will pass through the SATA and Ethernet 03:00.0
devices into the pre-launched RTVM, and give the rest of the devices to
the Service VM.

Install Service VM OS With GRUB on NVMe
=======================================

As with the Hybrid and Partition scenarios, the Pre-Launched RT
mode must boot using GRUB.  The ACRN hypervisor is loaded as a GRUB
multiboot kernel, while the pre-launched RTVM kernel and Service VM
kernel are loaded as multiboot modules. The ACRN hypervisor, Service
VM, and pre-launched RTVM kernel images are all located on the NVMe drive.
We recommend installing Ubuntu on the NVMe drive as the Service VM OS,
which also has the required GRUB image to launch Pre-Launched RT mode.
Refer to :ref:`gsg` to
install Ubuntu on the NVMe drive, and use GRUB to launch the Service VM.

Install Pre-Launched RT Filesystem on SATA and Kernel Image on NVMe
===================================================================

Follow the :ref:`gsg` to install RT rootfs on the SATA drive.

The kernel should
be on the NVMe drive along with GRUB. You'll need to copy the RT kernel
to the NVMe drive. Once you have successfully installed and booted
Ubuntu from the NVMe drive, you'll then need to copy the RT kernel from
the SATA to the NVMe drive:

.. code-block:: none

   sudo mount /dev/nvme0n1p1 /boot
   sudo mount /dev/sda1 /mnt
   sudo cp /mnt/bzImage /boot/EFI/BOOT/bzImage_RT

Build ACRN With Pre-Launched RT Mode
====================================

The ACRN VM configuration framework can easily configure resources for
pre-launched VMs. On Whiskey Lake WHL-IPC-I5, to pass through SATA and
Ethernet 03:00.0 devices to the pre-launched RTVM, build ACRN with:

.. code-block:: none

   make BOARD=whl-ipc-i5 SCENARIO=hybrid_rt

After the build completes, update ACRN on NVMe. It is
``/boot/EFI/BOOT/acrn.bin``, if ``/dev/nvme0n1p1`` is mounted at ``/boot``.

Add Pre-Launched RT Kernel Image to GRUB Config
===============================================

The last step is to modify the GRUB configuration file to load the pre-launched
RTVM kernel. (For more information about this, see
the :ref:`gsg`.) The GRUB configuration file will look something
like this:

.. code-block:: none

   menuentry 'ACRN multiboot2 hybrid'{
       echo 'loading multiboot2 hybrid...'
       multiboot2 /EFI/BOOT/acrn.bin
       module2 /EFI/BOOT/bzImage_RT RT_bzImage
       module2 /EFI/BOOT/bzImage Linux_bzImage
       module2 /boot/ACPI_VM0.bin ACPI_VM0
   }

Reboot the system, and it will boot into Pre-Launched RT Mode:

.. code-block:: none

   ACRN:\>vm_list
   VM_UUID                          VM_ID       VM_NAME                    VM_STATE
   ================================ ===== ================================ ========
   26c5e0d88f8a47d88109f201ebd61a5e   0   ACRN PRE-LAUNCHED VM0            Running
   dbbbd4347a574216a12c2201f1ab0240   1   ACRN Service VM                  Running
   ACRN:\>

Connect to the console of VM0, via ``vm_console`` ACRN shell command. (Press
:kbd:`Ctrl` + :kbd:`Space` to return to the ACRN shell.)

.. code-block:: none

   ACRN:\>vm_console 0

   ----- Entering VM 0 Shell -----

   root@clr-85a5e9fbac604fbbb92644991f6315df ~ #

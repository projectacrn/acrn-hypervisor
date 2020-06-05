.. _ivshmem-hld:

ACRN Shared Memory Based Inter-VM Communication
###############################################

ACRN supports inter-virtual machine communication based on a shared
memory mechanism, The ACRN device model and hypervisor emulate a virtual
PCI device (called an ``ivshmem`` device) to expose the base address and
size of this shared memory.

Inter-VM Communication Overview
*******************************

.. figure:: images/ivshmem-image1.png
   :align: center
   :name: ivshmem-architecture-overview

   ACRN shared memory based inter-vm communication architecture

The ``ivshmem`` device is emulated in the ACRN device model (dm-land)
and its shared memory region is allocated from the Service VM's memory
space.  This solution only supports communication between post-launched
VMs.

.. note:: In a future implementation, the ``ivshmem`` device could
   instead be emulated in the hypervisor (hypervisor-land) and the shared
   memory regions reserved in the hypervisor's memory space.  This solution
   would work for both pre-launched and post-launched VMs.


ivshmem dm:
   The ivshmem device model implements register virtualization
   and shared memory mapping. Will support notification/interrupt mechanism
   in future.

ivshmem server:
   A daemon for inter-VM notification capability,
   This is currently not implemented, so the inter-VM communication
   doesn't support a notification mechanism.

Ivshmem Device Introduction
***************************

The ``ivshmem`` device is a virtual standard PCI device consisting of
two Base Address Registers (BARs): BAR0 is used for emulating interrupt related registers,
and BAR2 is used for exposing shared memory region. The ``ivshmem`` device
doesn't support any extra capabilities.

Configuration Space Definition

+---------------+----------+----------+
| Register      | Offset   | Value    |
+===============+==========+==========+
| Vendor ID     | 0x00     | 0x1AF4   |
+---------------+----------+----------+
| Device ID     | 0x02     | 0x1110   |
+---------------+----------+----------+
| Revision ID   | 0x08     | 0x1      |
+---------------+----------+----------+
| Class Code    | 0x09     | 0x5      |
+---------------+----------+----------+


MMIO Registers Definition

.. list-table::
   :widths: auto
   :header-rows: 1

   * - Register
     - Offset
     - Read/Write
     - Description
   * - IVSHMEM\_IRQ\_MASK\_REG
     - 0x0
     - R/W
     - Interrupt Status register is used for legacy interrupt.
       ivshmem doesn't support interrupts, so this is reserved.
   * - IVSHMEM\_IRQ\_STA\_REG
     - 0x4
     - R/W
     - Interrupt Mask register is used for legacy interrupt.
       ivshmem doesn't support interrupts, so this is reserved.
   * - IVSHMEM\_IV\_POS\_REG
     - 0x8
     - RO
     - Inter-VM Position register is used to identify the VM ID.
       Currently its value is zero.
   * - IVSHMEM\_DOORBELL\_REG
     - 0xC
     - WO
     - Doorbell register is used to trigger an interrupt to the peer VM.
       ivshmem doesn't support interrupts.

Usage
*****

To support two post-launched VMs communicating via an ``ivshmem`` device,
add this line as an acrn-dm boot parameter::

  -s slot, ivshmem, shm_name, shm_size

where

-  ``-s slot`` - Specify the virtual PCI slot number

-  ``ivshmem`` - Virtual PCI device name

-  ``shm_name`` - Specify a shared memory name. Post-launched VMs with the
   same ``shm_name`` share a shared memory region.

-  ``shm_size`` - Specify a shared memory size. The two communicating
   VMs must define the same size.

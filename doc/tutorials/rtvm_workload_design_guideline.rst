.. _rtvm_workload_guideline:

Real-Time VM Application Design Guidelines
##########################################

An RTOS developer must be aware of the differences between running applications on a native
platform and a real time virtual machine (RTVM), especially issues impacting real time
performance. For example, a real time thread should avoid triggering any VM-Exit. If a VM-Exit
is triggered, the developer must account for an additional margin of CPU cycles for the
incremental runtime overhead.

This document provides some application design guidelines when using an RTVM within the ACRN hypervisor.

Run RTVM With Dedicated Resources/Devices
*****************************************

For best practice, ACRN allocates dedicated CPU, memory resources, and cache resources (using Intel
Resource Directory allocation Technology such as CAT, MBA) to RTVMs. For best real time performance
of I/O devices, we recommend using dedicated (passthrough) PCIe devices to avoid VM-Exit at run time.

.. note::
   The configuration space for passthrough PCI devices is still emulated and accessing it will
   trigger a VM-Exit.

RTVM With Virtio PMD (Polling Mode Driver) for I/O Sharing
**********************************************************

If the RTVM must use shared devices, we recommend using PMD drivers that can eliminate the
unpredictable latency caused by guest I/O trap-and-emulate access. The RTVM application must be
aware that the packets in the PMD driver may arrive or be sent later than expected.

RTVM With HV Emulated Device
****************************

ACRN uses hypervisor emulated virtual UART (vUART) devices for inter-VM synchronization such as
logging output or command send/receive.  The vUART only works in polling mode, but
may be extended to support interrupt mode in a future release. In the meantime, for better RT
behavior, the RT application using the vUART shall reserve a margin of CPU cycles to accommodate
for the additional latency introduced by the VM-Exit to the vUART I/O registers (~2000-3000 cycles
per register access).

DM Emulated Device (Except PMD)
*******************************

We recommend **not** using DM-emulated devices in an RTVM.

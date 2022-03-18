.. _partition-mode-hld:

Partition Mode
##############

ACRN is a type 1 hypervisor that supports running multiple guest operating
systems (OS). Typically, the platform BIOS/bootloader boots ACRN, and
ACRN loads single or multiple guest OSes. Refer to :ref:`hv-startup` for
details on the start-up flow of the ACRN hypervisor.

ACRN supports two modes of operation: sharing mode and partition mode.
This document describes ACRN's high-level design for partition mode
support.

.. contents::
   :depth: 2
   :local:

Introduction
************

In partition mode, ACRN provides guests with exclusive access to cores,
memory, cache, and peripheral devices. Partition mode enables developers
to dedicate resources exclusively among the guests. However, there is no
support today in x86 hardware or in ACRN to partition resources such as
peripheral buses (e.g., PCI). On x86 platforms that support Cache
Allocation Technology (CAT) and Memory Bandwidth Allocation (MBA), developers
can partition Level 2 (L2) cache, Last Level Cache (LLC), and memory bandwidth
among the guests. Refer to
:ref:`hv_rdt` for more details on ACRN RDT high-level design and
:ref:`rdt_configuration` for RDT configuration.


ACRN expects static partitioning of resources either by code
modification for guest configuration or through compile-time config
options. All the devices exposed to the guests are either physical
resources or are emulated in the hypervisor. There is no need for a
Device Model and Service VM. :numref:`pmode2vms` shows a partition mode
example of two VMs with exclusive access to physical resources.

.. figure:: images/partition-image3.png
   :align: center
   :name: pmode2vms

   Partition Mode Example with Two VMs

Guest Info
**********

ACRN uses multi-boot info passed from the platform bootloader to know
the location of each guest kernel in memory. ACRN creates a copy of each
guest kernel into each of the guests' memory. Current implementation of
ACRN requires developers to specify kernel parameters for the guests as
part of the guest configuration. ACRN picks up kernel parameters from the guest
configuration and copies them to the corresponding guest memory.

.. figure:: images/partition-image18.png
   :align: center

   Guest Info

ACRN Setup for Guests
*********************

Cores
=====

ACRN requires the developer to specify the number of guests and the
cores dedicated for each guest. Also, the developer needs to specify
the physical core used as the bootstrap processor (BSP) for each guest. As
the processors are brought to life in the hypervisor, it checks if they are
configured as BSP for any of the guests. If a processor is the BSP of any of
the guests, ACRN proceeds to build the memory mapping for the guest,
mptable, E820 entries, and zero page for the guest. As described in
`Guest info`_, ACRN creates copies of guest kernel and kernel
parameters into guest memory. :numref:`partBSPsetup` explains these
events in chronological order.

.. figure:: images/partition-image7.png
   :align: center
   :name: partBSPsetup

   Event Order for Processor Setup

Memory
======

For each guest in partition mode, the ACRN developer specifies the size of
memory for the guest and the starting address in the host physical
address in the guest configuration. There is no support for HIGHMEM for
partition mode guests. The developer needs to take care of two aspects
for assigning host memory to the guests:

1) Sum of guest PCI hole and guest "System RAM" is less than 4GB.

2) Pick the starting address in the host physical address and the
   size so that it does not overlap with any reserved regions in
   host E820.

ACRN creates EPT mapping for the guest between GPA (0, memory size) and
HPA (starting address in guest configuration, memory size).

E820 and Zero Page Info
=======================

A default E820 is used for all the guests in partition mode. This table
shows the reference E820 layout. Zero page is created with this
E820 info for all the guests.

+------------------------+
| RAM                    |
|                        |
| 0 - 0xEFFFFH           |
+------------------------+
| RESERVED (MPTABLE)     |
|                        |
| 0xF0000H - 0x100000H   |
+------------------------+
| RAM                    |
|                        |
| 0x100000H - LOWMEM     |
+------------------------+
| RESERVED               |
+------------------------+
| PCI HOLE               |
+------------------------+
| RESERVED               |
+------------------------+

Platform Info - mptable
=======================

ACRN, in partition mode, uses mptable to convey platform info to each
guest. Using this platform information, number of cores used for each
guest, and whether the guest needs devices with INTX, ACRN builds
mptable and copies it to the guest memory. In partition mode, ACRN uses
physical APIC IDs to pass to the guests.

I/O - Virtual Devices
=====================

Port I/O is supported for PCI device config space 0xcfc and 0xcf8, vUART
0x3f8, vRTC 0x70 and 0x71, and vPIC ranges 0x20/21, 0xa0/a1, and
0x4d0/4d1. MMIO is supported for vIOAPIC. ACRN exposes a virtual
host-bridge at BDF (Bus Device Function) 0.0:0 to each guest. Access to
256 bytes of config space for virtual host bridge is emulated.

I/O - Passthrough Devices
=========================

ACRN, in partition mode, supports passing through PCI devices on the
platform. All the passthrough devices are exposed as child devices under
the virtual host bridge. ACRN does not support either passing through
bridges or emulating virtual bridges. Passthrough devices should be
statically allocated to each guest using the guest configuration. ACRN
expects the developer to provide the virtual BDF to BDF of the
physical device mapping for all the passthrough devices as part of each guest
configuration.

Runtime ACRN Support for Guests
*******************************

ACRN, in partition mode, supports an option to pass through LAPIC of the
physical CPUs to the guest. ACRN expects developers to specify if the
guest needs LAPIC passthrough using guest configuration. When the guest
configures vLAPIC as x2APIC, and if the guest configuration has LAPIC
passthrough enabled, ACRN passes the LAPIC to the guest. The guest can access
the LAPIC hardware directly without hypervisor interception. During
runtime of the guest, this option differentiates how ACRN supports
inter-processor interrupt handling and device interrupt handling. This
will be discussed in detail in the corresponding sections.

.. figure:: images/partition-image16.png
   :align: center

   LAPIC Passthrough

Guest SMP Boot Flow
===================

The core APIC IDs are reported to the guest using mptable info. SMP boot
flow is similar to sharing mode. Refer to :ref:`vm-startup`
for guest SMP boot flow in ACRN. Partition mode guests startup is the same as
the Service VM startup in sharing mode.

Inter-Processor Interrupt (IPI) Handling
========================================

Guests Without LAPIC Passthrough
--------------------------------

For guests without LAPIC passthrough, IPIs between guest CPUs are handled in
the same way as sharing mode in ACRN. Refer to :ref:`virtual-interrupt-hld`
for more details.

Guests With LAPIC Passthrough
-----------------------------

ACRN supports passthrough if and only if the guest is using x2APIC mode
for the vLAPIC. In LAPIC passthrough mode, writes to the Interrupt Command
Register (ICR) x2APIC MSR are intercepted. The guest writes the IPI info,
including vector, and destination APIC IDs to the ICR. Upon an IPI request
from the guest, ACRN does a sanity check on the destination processors
programmed into the ICR. If the destination is a valid target for the guest,
ACRN sends an IPI with the same vector from the ICR to the physical CPUs
corresponding to the destination processor info in the ICR.

.. figure:: images/partition-image14.png
   :align: center

   IPI Handling for Guests With LAPIC Passthrough

Passthrough Device Support
==========================

Configuration Space Access
--------------------------

ACRN emulates Configuration Space Address (0xcf8) I/O port and
Configuration Space Data (0xcfc) I/O port for guests to access PCI
devices configuration space. Within the config space of a device, Base
Address registers (BAR), offsets starting from 0x10H to 0x24H, provide
the information about the resources (I/O and MMIO) used by the PCI
device. ACRN virtualizes the BAR registers and for the rest of the
config space, forwards reads and writes to the physical config space of
passthrough devices.  Refer to the `I/O`_ section below for more details.

.. figure:: images/partition-image1.png
   :align: center

   Configuration Space Access

DMA
---

ACRN developers need to statically define the passthrough devices for each
guest using the guest configuration. For devices to DMA to/from guest
memory directly, ACRN parses the list of passthrough devices for each
guest and creates context entries in the VT-d remapping hardware. EPT
page tables created for the guest are used for VT-d page tables.

I/O
---

ACRN supports I/O for passthrough devices with two restrictions.

1) Supports only MMIO. Thus, this requires developers to expose I/O BARs as
   not present in the guest configuration.

2) Supports only 32-bit MMIO BAR type.

As the guest PCI sub-system scans the PCI bus and assigns a Guest Physical
Address (GPA) to the MMIO BAR, ACRN maps the GPA to the address in the
physical BAR of the passthrough device using EPT. The following timeline chart
explains how PCI devices are assigned to the guest and how BARs are mapped upon
guest initialization.

.. figure:: images/partition-image13.png
   :align: center

   I/O for Passthrough Devices

Interrupt Configuration
-----------------------

ACRN supports both legacy (INTx) and MSI interrupts for passthrough
devices.

INTx Support
~~~~~~~~~~~~

ACRN expects developers to identify the interrupt line info (0x3CH) from
the physical BAR of the passthrough device and build an interrupt entry in
the mptable for the corresponding guest. As the guest configures the vIOAPIC
for the interrupt RTE, ACRN writes the info from the guest RTE into the
physical IOAPIC RTE. Upon the guest kernel request to mask the interrupt,
ACRN writes to the physical RTE to mask the interrupt at the physical
IOAPIC. When the guest masks the RTE in vIOAPIC, ACRN masks the interrupt
RTE in the physical IOAPIC. Level triggered interrupts are not
supported.

MSI Support
~~~~~~~~~~~

The guest reads/writes to the PCI configuration space to configure MSI
interrupts using an address. Data and control registers are passed through to
the physical BAR of the passthrough device. Refer to `Configuration
Space Access`_ for details on how the PCI configuration space is emulated.

Virtual Device Support
======================

ACRN provides read-only vRTC support for partition mode guests. Writes
to the data port are discarded.

For port I/O to ports other than vPIC, vRTC, or vUART, reads return 0xFF and
writes are discarded.

Interrupt Delivery
==================

Guests Without LAPIC Passthrough
--------------------------------

In ACRN partition mode, interrupts stay disabled after a vmexit.  The
processor does not take interrupts when it is executing in VMX root
mode. ACRN configures the processor to take vmexit upon external
interrupt if the processor is executing in VMX non-root mode. Upon an
external interrupt, after sending EOI to the physical LAPIC, ACRN
injects the vector into the vLAPIC of the vCPU running on the
processor. Guests using a Linux kernel use vectors less than 0xECh
for device interrupts.

.. figure:: images/partition-image20.png
   :align: center

   Interrupt Delivery for Guests Without LAPIC Passthrough

Guests With LAPIC Passthrough
-----------------------------

For guests with LAPIC passthrough, ACRN does not configure vmexit upon
external interrupts. There is no vmexit upon device interrupts and they are
handled by the guest IDT.

Hypervisor IPI Service
======================

ACRN needs IPIs for events such as flushing TLBs across CPUs, sending virtual
device interrupts (e.g., vUART to vCPUs), and others.

Guests Without LAPIC Passthrough
--------------------------------

Hypervisor IPIs work the same way as in sharing mode.

Guests With LAPIC Passthrough
-----------------------------

Since external interrupts are passed through to the guest IDT, IPIs do not
trigger vmexit. ACRN uses NMI delivery mode and the NMI exiting is
chosen for vCPUs. At the time of NMI interrupt on the target processor,
if the processor is in non-root mode, vmexit happens on the processor
and the event mask is checked for servicing the events.

Debug Console
=============

For details on how the hypervisor console works, refer to
:ref:`hv-console`.

For a guest console in partition mode, ACRN provides an option to pass
``vmid`` as an argument to ``vm_console``. vmid is the same as the one
developers use in the guest configuration.

Guests Without LAPIC Passthrough
--------------------------------

Works the same way as sharing mode.

Hypervisor Console
==================

ACRN uses the TSC deadline timer to provide a timer service. The hypervisor
console uses a timer on CPU0 to poll characters on the serial device. To
support LAPIC passthrough, the TSC deadline MSR is passed through and the local
timer interrupt is also delivered to the guest IDT. Instead of the TSC
deadline timer, ACRN uses the VMX preemption timer to poll the serial device.

Guest Console
=============

ACRN exposes vUART to partition mode guests. vUART uses vPIC to inject an
interrupt to the guest BSP. If the guest has more than one core,
during runtime, vUART might need to inject an interrupt to the guest BSP from
another core (other than BSP). As mentioned in section `Hypervisor IPI
Service`_, ACRN uses NMI delivery mode for notifying the CPU running the BSP
of the guest.

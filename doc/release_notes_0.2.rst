.. _release_notes_0.2:

ACRN v0.2 (Sep 2018) DRAFT
##########################

We are pleased to announce the release of Project ACRN version 0.2.

ACRN is a flexible, lightweight reference hypervisor, built with
real-time and safety-criticality in mind, optimized to streamline
embedded development through an open source platform. Check out the
:ref:`introduction` for more information.

The project ACRN reference code can be found on GitHub in
https://github.com/projectacrn.  It includes the ACRN hypervisor, the
ACRN device model, and documentation.

Version 0.2 new features
************************

VT-x, VT-d
================
Based on Intel VT-x virtualization technology, ACRN emulates a virtual 
CPU with core partition and simple schedule. VT-d provides hardware 
support for isolating and restricting device accesses to the owner of 
the partition managing the device.It allows assigning I/O devices to a VM, 
and extending the protection and isolation properties of VMs for I/O operations.

PIC/IOAPIC/MSI/MSI-X/PCI/LAPIC
================================
ACRN hypervisor supports virtualized APIC-V/EPT/IOAPIC/LAPIC functionality.

Ethernet
================
ACRN hypervisor supports virtualized Ethernet functionality. Ethernet Mediator is 
executed in the Service OS and provides packet forwarding between the physical 
networking devices (Ethernet, Wi-Fi, etc.) and virtual devices in the Guest 
VMs(also called "User OS"). Virtual Ethernet device could be shared by Linux, 
Android, and Service OS guests for regular (i.e. non-AVB) traffic. All hypervisor 
para-virtualized I/O is implemented using the VirtIO specification Ethernet pass-through.

Storage (eMMC)
================
ACRN hypervisor supports virtualized non-volatile R/W storage for the Service 
OS and Guest OS instances, supporting VM private storage and/or storage shared 
between Guest OS instances.

USB (xDCI)
================
ACRN hypervisor supports virtualized assignment of all USB xHCI and/or xDCI 
controllers to a Guest OS from the platform.

USB Mediator (xHCI and DRD)
===========================
ACRN hypervisor supports a virtualized USB Mediator.

CSME
================
ACRN hypervisor supports a CSME to a single Linux, Android, or RTOS guest or 
the Service OS even when in a virtualized environment.

WiFi
================
ACRN hypervisor supports the passthrough assignment of the WiFi subsystem to the IVI, 
enables control of the WiFi as an in-vehicle hotspot for 3rd party devices, 
provides 3rd party device applications access to the vehicle, 
and provides access of 3rd party devices to the TCU provided connectivity.

IPU (MIPI-CS2, HDMI-in)
========================
ACRN hypervisor supports passthrough IPU assignment to Service OS or 
guest OS, without sharing.

Bluetooth
================
ACRN hypervisor supports bluetooth controller passthrough to a single Guest OS (IVI).

GPU  – Preemption
==================
GPU Preemption is one typical automotive use case which requires
the system to preempt GPU resources occupied by lower priority
workloads.  This is done to ensure performance of the most critical
workload can be achieved. Three different schedulers for the GPU
are involved: i915 UOS scheduler, Mediator GVT scheduler, and
i915 SOS scheduler.

GPU – display surface sharing via Hyper DMA
============================================
Surface sharing is one typical automotive use case which requires 
that the SOS accesses an individual surface or a set of surfaces 
from the UOS without having to access the entire frame buffer of 
the UOS. Hyper DMA Buffer sharing extends the Linux DMA buffer 
sharing mechanism where one driver is able to share its pages 
with another driver within one domain.

S3
================
ACRN hypervisor supports S3 feature, partially enabled in LaaG.


Fixed Issues
************

:acrn-issue:`663` - Black screen displayed after booting SOS/UOS

:acrn-issue:`676` - Hypervisor and DM version numbers incorrect

:acrn-issue:`1126` - VPCI coding style and bugs fixes for partition mode

:acrn-issue:`1125` - VPCI coding style and bugs fixes found in integration testing for partition mode

:acrn-issue:`1101` - missing acrn_mngr.h

:acrn-issue:`1071` - hypervisor cannot boot on skylake i5-6500

:acrn-issue:`1003` - CPU: cpu info is not correct

:acrn-issue:`971` -  acrncrashlog funcitons need to be enhance

:acrn-issue:`843` - ACRN boot failure

:acrn-issue:`721` - DM for IPU mediation

:acrn-issue:`707` - Issues found with instructions for using Ubuntu as SOS

:acrn-issue:`706` - Invisible mouse cursor in UOS

:acrn-issue:`424` - ClearLinux desktop GUI of SOS fails to launch


Known Issues
************

.. comment
   Use the syntax:

   :acrn-issue:`663` - Black screen displayed after booting SOS/UOS
     The ``weston`` display server, window manager, and compositor used by ACRN
     (from Clear Linux) may not have been properly installed and started.
     **Workaround** is described in ACRN GitHub issue :acrn-issue:`663`.


Change Log
**********

These commits have been added to the acrn-hypervisor repo since the v0.1
release in July 2018 (click on the CommitID link to see details):

.. comment

   This list is obtained from the command:
   git log --pretty=format:'- :acrn-commit:`%h` %s' --after="2018-03-01"

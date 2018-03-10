.. _release_notes:

Release Notes
#############

Version 0.1 release (March 2018)
********************************

In March 2018, Intel announced the open source Project ACRN at the
`Embedded Linux Conference and OpenIoT Summit North America 2018
<ELC18>`.

.. _ELC18:
   https://events.linuxfoundation.org/events/elc-openiot-north-america-2018/

ACRN is a flexible, lightweight reference hypervisor, built with
real-time and safety-criticality in mind, optimized to streamline
embedded development through an open source platform.

The project ACRN reference code can be found on GitHug in https://github.com/projectacrn
It includes the ACRN hypervisor, the ACRN device model and
documentation.
ACRN is a flexible, lightweight hypervisor, built with real-time and
safety-criticality in mind,it is optimized to streamline embedded
development through an open source reference platform. ACRNs key
features include:

* Small footprint
* Built with Real-Time in Mind
* Virtualization of Embedded IoT Device functions
* Safety Critical Workload Considerations
* Adaptable
* Open Source

This version 0.1 release has the following software components:

* The ACRN Hypervisor
* The ACRN Device Model
* The ACRN VirtIO framework
* The ACRN Block & NIC & console VirtIO drivers
* The ACRN VirtIO Backend Service(VBS) & VirIO and Hypervisor Service Module(VHM).

Version 0.1 features include:

* ACRN hypervisor (Type 1 hypervisor)
* A hybrid VMM architecture implementation
* Multiple User OS supported
* VM management - such as VM start/stop/pause, virtual CPU pause/resume
* CPU virtualization
* Memory virtualization
* I/O emulation
* Virtual interrupt
* VT-x and VT-d support
* Hypercall for guest
* Device emulation
* Device pass-through mechanism
* Device Emulation mechanism
* Virtio console
* Virt-network

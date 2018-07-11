.. _release_notes:

Release Notes
#############

Version 0.1 release (July 2018)
********************************

ACRN is a flexible, lightweight reference hypervisor, built with
real-time and safety-criticality in mind, optimized to streamline
embedded development through an open source platform. Check out the
:ref:`introduction` for more information.

The project ACRN reference code can be found on GitHub in
`https://github.com/projectacrn/acrn-hypervisor`_.  It includes the
ACRN hypervisor, the ACRN device model, ACRN tools, and documentation.

ACRN's key features include:

* Small footprint
* Built with real-time in mind
* Virtualization of embedded IoT device functions
* Safety-critical workload considerations
* Adaptable
* Open Source

This version 0.1 release has the following software components:

* The ACRN Hypervisor
* The ACRN Device Model
* The ACRN Graphics Sharing (GVT-g)
* The ACRN Virtio framework
* The ACRN Block, NIC, and console Virtio drivers
* The ACRN Virtio Backend Service(VBS) and the Virtio and Hypervisor
  Service Module (VHM)
* The ACRN tools

The Service OS and User OS are based on `Clear Linux <https://clearlinux.org/>`_
version 23510.

Version 0.1 features include:

- Acrnctl
- Acrntrace
- Acrnlog
- SOS lifecycle
- vSBL
- virtio-blk
- virtio-net
- USB pass-thru
- CSE pass-thru
- IOC sharing (incl. cbc attach, cbc driver)
- IPU pass-thru
- BT pass-thru
- SD card pass-thru
- audio pass-thru
- surface sharing
- multi-plane, multi-pipe
- HDMI, eDP

Known Issues
============

* GPU - Preemption (Prioritized Rendering, Batch Preemption, QoS Rendering)
* Preemption feature works, but performance is not optimized yet.
* Wifi not supported in guest OS
* Audio pass-through to guest OS, but can only be validated on driver level
  using command line or alsa-player, and only supports limited formats
* CSME pass-through to guest OS
* GVT-g is available, need to perform features after configured properly
* Surface Sharing: Sometimes the window setup on Service OS takes up to 30 second
* Sometimes system hangs, especially when workload is high (e.g. running benchmarks, playing videos)

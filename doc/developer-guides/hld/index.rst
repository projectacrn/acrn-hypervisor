.. _hld:

High-Level Design Guides
########################

The ACRN Hypervisor acts as a host with full control of the processors
and the hardware (physical memory, interrupt management, and I/O). It
provides the User OS with an abstraction of a virtual platform, allowing
the guest to behave as if it were executing directly on a physical
processor.

These chapters describe the ACRN architecture, high-level design,
background, and motivation for specific areas within the ACRN hypervisor
system.

.. rst-class:: rst-columns

.. toctree::
   :maxdepth: 2

   Overview <hld-overview>
   Hypervisor <hld-hypervisor>
   Device Model <hld-devicemodel>
   Emulated Devices <hld-emulated-devices>
   Virtio Devices <hld-virtio-devices>
   Power Management <hld-power-management>
   Tracing and Logging <hld-trace-log>
   Security <hld-security>

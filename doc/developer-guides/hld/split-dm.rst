.. _split-device-model:

Split Device Model
==================

We usually emulate device in Device Model. However, we need to emulate device in
ACRN Hypervisor for some reasons. For example, post-launched RTVM needs to emulate
pass through PCI(e) devices in ACRN Hypervisor in which case it could continue to
running even if the Device Model is killed. In spite of this, the Device Model still
should own the overall resouce management like memroy/MMIO space, interrupt pin etc.
So there shall be one communication method provided by ACRN Hypervisor, which used
to align the resource information for Device Model to ACRN Hypervisor emulated device.

Let's still take the pass through PCI(e) device as an example:
Before we split pass through PCI(e) device from Device Model to ACRN Hypervisor, the
whole picture is like this:

.. figure:: images/split-dm-image1.png
   :align: center
   :width: 900px
   :name: split-dm-architecture-overview1

   PCI Config space access in Service VM or Pre-launched VM

.. figure:: images/split-dm-image2.png
   :align: center
   :width: 900px
   :name: split-dm-architecture-overview2

   PCI Config space access in Post-launched VM

After we split pass through PCI(e) device from Device Model to ACRN Hypervisor, the
whole picture is like this:

.. figure:: images/split-dm-image3.png
   :align: center
   :width: 900px
   :name: split-dm-architecture-overview3

.. figure:: images/split-dm-image4.png
   :align: center
   :width: 900px
   :name: split-dm-architecture-overview4

   PCI Config space access in Post-launched VM

Interfaces Design
=================

In order to achieve this, we add a new pair of hypercall to align the PCI(e) BAR
and INTx information.

.. doxygenfunction:: hcall_assign_pcidev
   :project: Project ACRN

.. doxygenfunction:: hcall_deassign_pcidev
   :project: Project ACRN


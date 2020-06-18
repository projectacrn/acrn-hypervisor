.. _release_notes_2.0:

ACRN v2.0 (June 2020)
#####################

We are pleased to announce the second major release of the Project ACRN
hypervisor.

ACRN v2.0 offers new and improved scenario definitions, with a focus on
industrial IoT and edge device use cases. ACRN supports these uses with
their demanding and varying workloads including Functional Safety
certification, real-time characteristics, device and CPU sharing, and
general computing power needs, while honoring required isolation and
resource partitioning. A wide range of User VM OSs (such as Windows 10,
Ubuntu, Android, and VxWorks) can run on ACRN, running different
workloads and applications on the same hardware platform.

Workload management and orchestration, rather standard and mature in
cloud environments, are enabled now in ACRN, allowing open source
orchestrators such as OpenStack to manage ACRN virtual machines. Kata
Containers, a secure container runtime, has also been enabled on ACRN
and can be orchestrated via Docker or Kubernetes.

Rounding things out, we've also made significant improvements in
configuration tools, added many new tutorial documents, and enabled ACRN
on the QEMU machine emulator making it easier to try out and develop with
ACRN.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline
embedded development through an open source platform. Check out
:ref:`introduction` for more information.  All project ACRN source code
is maintained in the https://github.com/projectacrn/acrn-hypervisor
repository and includes folders for the ACRN hypervisor, the ACRN device
model, tools, and documentation. You can either download this source
code as a zip or tar.gz file (see the `ACRN v2.0 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v2.0>`_)
or use Git clone and checkout commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v2.0

The project's online technical documentation is also tagged to
correspond with a specific release: generated v2.0 documents can be
found at https://projectacrn.github.io/2.0/.  Documentation for the
latest (master) branch is found at
https://projectacrn.github.io/latest/.
Follow the instructions in the :ref:`rt_industry_ubuntu_setup` to get
started with ACRN.

We recommend that all developers upgrade to ACRN release v2.0.

Version 2.0 Key Features (comparing with v1.0)
**********************************************

.. contents::
   :local:
   :backlinks: entry

ACRN Architecture Upgrade to Support Hybrid Mode
================================================

The ACRN architecture has evolved after its initial major 1.0 release in
May 2019.  The new hybrid mode architecture has the flexibility to
support both partition mode and sharing mode simultaneously, as shown in
this architecture diagram:

.. figure:: ../introduction/images/ACRN-V2-high-level-arch.png
   :width: 700px
   :align: center

   ACRN V2 high-level architecture

On the left, resources are partitioned and used by a pre-launched User
Virtual Machine (VM), started by the hypervisor before the Service VM
has been launched. It runs independent of other virtual machines, and
can own its own dedicated hardware resources, such as a CPU core,
memory, and I/O devices. Because other VMs may not even be aware of its
existence, this pre-launched VM can be used as a safety VM where, for
example, platform hardware failure detection code can run and take
emergency actions if a system critical failure occurs.

On the right, the remaining hardware resources are shared by the Service
VM and User VMs.  The Service VM can access hardware resources directly
(by running native drivers) and offer device sharing services to other
User VMs by the Device Model.

Also on the right, a special post-launched real-time VM (RTVM) can run a
hard real-time OS, such as VxWorks*, Zephyr*, or Xenomai*. Because of
its real-time capabilities, the RTVM can be used for soft PLC, IPC, or
Robotics applications.

New Hardware Platform Support
=============================

This release adds support for 8th Gen Intel® Core™ Processors (code
name: Whiskey Lake). (See :ref:`hardware` for platform details.)

Pre-launched Safety VM Support
==============================

ACRN supports a pre-launched partitioned safety VM, isolated from the
Service VM and other post-launched VM by using partitioned HW resources.
For example, in the hybrid mode, a real-time Zephyr RTOS VM can be
"pre-launched" by the hypervisor even before the Service VM is launched,
and with its own dedicated resources to achieve a high level of
isolation. This is designed to meet the needs of a Functional Safety OS.

Post-launched VM support via OVMF
=================================

ACRN supports Open Virtual Machine Firmware (OVMF) as a virtual boot
loader for the Service VM to launch post-launched VMs such as Windows,
Linux, VxWorks, or Zephyr RTOS. Secure boot is also supported.

Post-launched Real-Time VM Support
==================================

ACRN supports a post-launched RTVM, which also uses partitioned hardware
resources to ensure adequate real-time performance, as required for
industrial use cases.

Real-Time VM Performance Optimizations
======================================

ACRN 2.0 improves RTVM performance with these optimizations:

* **Eliminate use of VM-Exit and its performance overhead:**
   Use Local APIC (LAPIC) passthrough, Virtio Polling Mode Drivers (PMD),
   and NMI interrupt notification technologies.

* **Isolate the RTVM from the Service VM:**
   The ACRN hypervisor uses RDT (Resource Director Technology)
   allocation features such as CAT (Cache Allocation Technology), CDP (Code
   Data Prioritization), and MBA (Memory Bandwidth Allocation) to provide
   better isolation and prioritize critical resources, such as cache and
   memory bandwidth, for RTVMs over other VMs.

* **PCI Configuration space access emulation for passthrough devices in the hypervisor:**
   The hypervisor provides the necessary emulation (such as config space)
   of the passthrough PCI device during runtime for a DM-launched VM from
   Service VM.

* **PCI bridge emulation inside hypervisor**

* **ART (Always Running Timer Virtualization):**
   Ensure time is synchronized between Ptdev and vART

CPU Sharing Support
===================

ACRN supports CPU Sharing to fully utilize the physical CPU resource
across more virtual machines. ACRN enables a borrowed virtual time CPU
scheduler in the hypervisor to make sure the physical CPU can be shared
between VMs and support for yielding an idle vCPU when it's running a
'HLT' or 'PAUSE' instruction.

Many choices for User VM OS
===========================

ACRN now supports Windows* 10, Android*, Ubuntu*, Xenomai, VxWorks*,
Real-Time Linux*, and Zephyr* RTOS.  ACRN's Windows support now conforms
to the Microsoft* Hypervisor Top-Level Functional Specification (TLFS).
ACRN 2.0 also improves overall Windows as a Guest (WaaG) stability and
performance.

SR-IOV Support
==============

SR-IOV (Single Root Input/Output Virtualization) can isolate PCIe
devices to offer performance similar to bare-metal levels. For a
network adapter, for example, this enables network traffic to bypass the
software switch layer in the virtualization stack and achieve network
performance that is nearly the same as in a nonvirtualized environment.
In this example, the ACRN Service VM supports a SR-IOV ethernet device
through the Physical Function (PF) driver, and ensures that the SR-IOV
Virtual Function (VF) device can passthrough to a post-launched VM.

Graphics passthrough support
============================

ACRN supports GPU passthrough to dedicated User VM based on Intel GVT-d
technology used to virtualize the GPU for multiple guest VMs,
effectively providing near-native graphics performance in the VM.

Shared memory based Inter-VM communication
==========================================

ACRN supports Inter-VM communication based on shared memory for
post-launched VMs communicating via a Userspace I/O (UIO) interface.

Configuration Tool Support
==========================

A new offline configuration tool helps developers deploy ACRN to
different hardware systems with its own customization.

Kata Containers Support
=======================

ACRN can launch a Kata container, a secure container runtime,  as a User VM.

VM orchestration
================

Libvirt is an open-source API, daemon, and management tool as a layer to
decouple orchestrators and hypervisors. By adding a "ACRN driver", ACRN
supports libvirt-based tools and orchestrators to configure a User VM's CPU
configuration during VM creation.

Document updates
================
Many new and updated `reference documents <https://projectacrn.github.io>`_ are available, including:

* General

  * :ref:`introduction`
  * :ref:`hardware`
  * :ref:`asa`

* Getting Started

  * :ref:`rt_industry_ubuntu_setup`
  * :ref:`using_partition_mode_on_nuc`

* Configuration and Tools

  * :ref:`acrn_configuration_tool`

* Service VM Tutorials

  * :ref:`running_deb_as_serv_vm`

* User VM Tutorials

  .. rst-class:: rst-columns2

  * :ref:`using_zephyr_as_uos`
  * :ref:`running_deb_as_user_vm`
  * :ref:`using_celadon_as_uos`
  * :ref:`using_windows_as_uos`
  * :ref:`using_vxworks_as_uos`
  * :ref:`using_xenomai_as_uos`

* Enable ACRN Features

  .. rst-class:: rst-columns2

  * :ref:`open_vswitch`
  * :ref:`rdt_configuration`
  * :ref:`sriov_virtualization`
  * :ref:`cpu_sharing`
  * :ref:`run-kata-containers`
  * :ref:`how-to-enable-secure-boot-for-windows`
  * :ref:`enable-s5`
  * :ref:`vuart_config`
  * :ref:`sgx_virt`
  * :ref:`acrn-dm_qos`
  * :ref:`setup_openstack_libvirt`
  * :ref:`acrn_on_qemu`
  * :ref:`gpu-passthrough`

* Debug

  * :ref:`rt_performance_tuning`
  * :ref:`rt_perf_tips_rtvm`

* High-Level Design Guides

  * :ref:`virtio-i2c`
  * :ref:`split-device-model`
  * :ref:`hv-device-passthrough`
  * :ref:`vtd-posted-interrupt`


Fixed Issues
************

.. comment- :acrn-issue:`1773` - [APLNUC][IO][LaaG]USB Mediator USB3.0 and USB2.0 flash disk boot up UOS, quickly hot plug USB and Can not recognize all the devices

Known Issues
************

.. comment- :acrn-issue:`4046` - [WHL][Function][WaaG] Error info popoup when run 3DMARK11 on Waag

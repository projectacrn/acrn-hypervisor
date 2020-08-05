.. _introduction:

What is ACRN
############

Introduction to Project ACRN
****************************

ACRN |trade| is a, flexible, lightweight reference hypervisor, built with
real-time and safety-criticality in mind, and optimized to streamline
embedded development through an open source platform. ACRN defines a
device hypervisor reference stack and an architecture for running
multiple software subsystems, managed securely, on a consolidated system
by means of a virtual machine manager (VMM). It also defines a reference
framework implementation for virtual device emulation, called the "ACRN
Device Model".

The ACRN Hypervisor is a Type 1 reference hypervisor stack, running
directly on the bare-metal hardware, and is suitable for a variety of
IoT and embedded device solutions. The ACRN hypervisor addresses the gap
that currently exists between datacenter hypervisors, and hard
partitioning hypervisors. The ACRN hypervisor architecture partitions
the system into different functional domains, with carefully selected
user VM sharing optimizations for IoT and embedded devices.

ACRN Open Source Roadmap
************************

Stay informed on what's ahead for ACRN by visiting the
`ACRN Project Roadmap <https://projectacrn.org/#resources>`_ on the
projectacrn.org website.

For up-to-date happenings, visit the `ACRN blog <https://projectacrn.org/blog/>`_.

ACRN High-Level Architecture
****************************

The ACRN architecture has evolved since its initial v0.1 release in
July 2018. Beginning with the v1.1 release, the ACRN architecture has
flexibility to support partition mode, sharing mode, and a mixed hybrid
mode. As shown in :numref:`V2-hl-arch`, hardware resources can be
partitioned into two parts:

.. figure:: images/ACRN-V2-high-level-arch.png
   :width: 700px
   :align: center
   :name: V2-hl-arch

   ACRN high-level architecture

Shown on the left of :numref:`V2-hl-arch`, resources are partitioned and
used by a pre-launched user virtual machine (VM). Pre-launched here
means that it is launched by the hypervisor directly, even before the
service VM is launched. The pre-launched VM runs independently of other
virtual machines and owns dedicated hardware resources, such as a CPU
core, memory, and I/O devices. Other virtual machines may not even be
aware of the pre-launched VM's existence. Because of this, it can be
used as a safety OS virtual machine.  Platform hardware failure
detection code runs inside this pre-launched VM and will take emergency
actions when system critical failures occur.

Shown on the right of :numref:`V2-hl-arch`, the remaining hardware
resources are shared among the service VM and user VMs.  The service VM
is similar to Xen's Dom0, and a user VM is similar to Xen's DomU. The
service VM is the first VM launched by ACRN, if there is no pre-launched
VM. The service VM can access hardware resources directly by running
native drivers and it provides device sharing services to the user VMs
through the Device Model.  Currently, the service VM is based on Linux,
but it can also use other operating systems as long as the ACRN Device
Model is ported into it. A user VM can be Clear Linux*, Ubuntu*, Android*,
Windows* or VxWorks*.  There is one special user VM, called a
post-launched Real-Time VM (RTVM), designed to run a hard real-time OS,
such as Zephyr*, VxWorks*, or Xenomai*. Because of its real-time capability, RTVM
can be used for soft programmable logic controller (PLC), inter-process
communication (IPC), or Robotics applications.

.. _usage-scenarios:

Usage Scenarios
***************

ACRN can be used for heterogeneous workload consolidation in
resource-constrained embedded platform, targeting for functional safety,
or hard real-time support. It can take multiple separate systems and
enable a workload consolidation solution operating on a single compute
platform to run both safety-critical applications and non-safety
applications, together with security functions that safeguard the
system.


Automotive Application Scenarios
================================

As shown in :numref:`V2-SDC-scenario`, the ACRN hypervisor can be used
for building Automotive Software Defined Cockpit (SDC) and In-Vehicle
Experience (IVE) solutions.

.. figure:: images/ACRN-V2-SDC-scenario.png
   :width: 600px
   :align: center
   :name: V2-SDC-scenario

   ACRN Automotive SDC scenario

As a reference implementation, ACRN provides the basis for embedded
hypervisor vendors to build solutions with a reference I/O mediation
solution.  In this scenario, an automotive SDC system consists of the
Instrument Cluster (IC) system running in the Service VM and the In-Vehicle
Infotainment (IVI) system is running the post-launched User VM. Additionally,
one could modify the SDC scenario to add more post-launched User VMs that can
host Rear Seat Entertainment (RSE) systems (not shown on the picture).

An **Instrument Cluster (IC)** system is used to show the driver operational
information about the vehicle, such as:

- the speed, fuel level, trip mileage, and other driving information of
  the car;
- projecting heads-up images on the windshield, with alerts for low
  fuel or tire pressure;
- showing rear-view and surround-view cameras for parking assistance.

An **In-Vehicle Infotainment (IVI)** system's capabilities can include:

- navigation systems, radios, and other entertainment systems;
- connection to mobile devices for phone calls, music, and applications
  via voice recognition;
- control interaction by gesture recognition or touch.

A **Rear Seat Entertainment (RSE)** system could run:

- entertainment system;
- virtual office;
- connection to the front-seat IVI system and mobile devices (cloud
  connectivity);
- connection to mobile devices for phone calls, music, and applications
  via voice recognition;
- control interaction by gesture recognition or touch.

The ACRN hypervisor can support both Linux* VM and Android* VM as User
VMs managed by the ACRN hypervisor. Developers and OEMs can use this
reference stack to run their own VMs, together with IC, IVI, and RSE
VMs. The Service VM runs in the background and the User VMs run as
Post-Launched VMs.

A block diagram of ACRN's SDC usage scenario is shown in
:numref:`V2-SDC-scenario` above.

- The ACRN hypervisor sits right on top of the bootloader for fast booting
  capabilities.
- Resources are partitioned to ensure safety-critical and
  non-safety-critical domains are able to coexist on one platform.
- Rich I/O mediators allows sharing of various I/O devices across VMs,
  delivering a comprehensive user experience.
- Multiple operating systems are supported by one SoC through efficient
  virtualization.

Industrial Workload Consolidation
=================================

.. figure:: images/ACRN-V2-industrial-scenario.png
   :width: 600px
   :align: center
   :name: V2-industrial-scenario

   ACRN Industrial Workload Consolidation scenario

Supporting Workload consolidation for industrial applications is even
more challenging. The ACRN hypervisor needs to run different workloads with no
interference, increase security functions that safeguard the system, run hard
real-time sensitive workloads together with general computing workloads, and
conduct data analytics for timely actions and predictive maintenance.

Virtualization is especially important in industrial environments
because of device and application longevity. Virtualization enables
factories to modernize their control system hardware by using VMs to run
older control systems and operating systems far beyond their intended
retirement dates.

As shown in :numref:`V2-industrial-scenario`, the Service VM can start a number
of post-launched User VMs and can provide device sharing capabilities to these.
In total, up to 7 post-launched User VMs can be started:

- 5 regular User VMs,
- One `Kata Containers <https://katacontainers.io>`_ User VM (see
  :ref:`run-kata-containers` for more details), and
- One Real-Time VM (RTVM).

In this example, one post-launched User VM provides Human Machine Interface
(HMI) capability, another provides Artificial Intelligence (AI) capability, some
compute function is run the Kata Container and the RTVM runs the soft
Programmable Logic Controller (PLC) that requires hard real-time
characteristics.

:numref:`V2-industrial-scenario` shows ACRN's block diagram for an
Industrial usage scenario:

- ACRN boots from the SoC platform, and supports firmware such as the
  UEFI BIOS.
- The ACRN hypervisor can create VMs that run different OSes:

  - a Service VM such as Ubuntu*,
  - a Human Machine Interface (HMI) application OS such as Windows*,
  - an Artificial Intelligence (AI) application on Linux*,
  - a Kata Container application, and
  - a real-time control OS such as Zephyr*, VxWorks* or RT-Linux*.

- The Service VM, provides device sharing functionalities, such as
  disk and network mediation, to other virtual machines.
  It can also run an orchestration agent allowing User VM orchestration
  with tools such as Kubernetes*.
- The HMI Application OS can be Windows* or Linux*. Windows is dominant
  in Industrial HMI environments.
- ACRN can support a soft Real-time OS such as preempt-rt Linux for
  soft-PLC control, or a hard Real-time OS that offers less jitter.

Best Known Configurations
*************************

The ACRN Github codebase defines five best known configurations (BKC)
targeting SDC and Industry usage scenarios. Developers can start with
one of these pre-defined configurations and customize it to their own
application scenario needs.

.. list-table:: Scenario-based Best Known Configurations
   :header-rows: 1

   * - Pre-defined BKC
     - Usage Scenario
     - VM0
     - VM1
     - VM2
     - VM3

   * - Software Defined Cockpit
     - SDC
     - Service VM
     - Post-launched VM
     - One Kata Containers VM
     -

   * - Industry Usage Config
     - Industry
     - Service VM
     - Up to 5 Post-launched VMs
     - One Kata Containers VM
     - Post-launched RTVM (Soft or Hard realtime)

   * - Hybrid Usage Config
     - Hybrid
     - Pre-launched VM (Safety VM)
     - Service VM
     - Post-launched VM
     -

   * - Logical Partition
     - Logical Partition
     - Pre-launched VM (Safety VM)
     - Pre-launched VM (QM Linux VM)
     -
     -

Here are block diagrams for each of these four scenarios.

SDC scenario
============

In this SDC scenario, an Instrument Cluster (IC) system runs with the
Service VM and an In-Vehicle Infotainment (IVI) system runs in a user
VM.

.. figure:: images/ACRN-V2-SDC-scenario.png
   :width: 600px
   :align: center
   :name: ACRN-SDC

   SDC scenario with two VMs

Industry scenario
=================

In this Industry scenario, the Service VM provides device sharing capability for
a Windows-based HMI User VM. One post-launched User VM can run a Kata Container
application. Another User VM supports either hard or soft Real-time OS
applications. Up to five additional post-launched User VMs support functions
such as Human Machine Interface (HMI), Artificial Intelligence (AI), Computer
Vision, etc.

.. figure:: images/ACRN-Industry.png
   :width: 600px
   :align: center
   :name: Industry

   Industry scenario

Hybrid scenario
===============

In this Hybrid scenario, a pre-launched Safety/RTVM is started by the
hypervisor. The Service VM runs a post-launched User VM that runs non-safety or
non-real-time tasks.

.. figure:: images/ACRN-Hybrid.png
   :width: 600px
   :align: center
   :name: ACRN-Hybrid

   Hybrid scenario

Logical Partition scenario
==========================

This scenario is a simplified VM configuration for VM logical
partitioning: one is the Safety VM and the other is a Linux-based User
VM.

.. figure:: images/ACRN-Logical-Partition.png
   :width: 600px
   :align: center
   :name: logical-partition

   Logical Partitioning scenario


Licensing
*********
.. _BSD-3-Clause: https://opensource.org/licenses/BSD-3-Clause

Both the ACRN hypervisor and ACRN Device model software are provided
under the permissive `BSD-3-Clause`_ license, which allows
*"redistribution and use in source and binary forms, with or without
modification"* together with the intact copyright notice and
disclaimers noted in the license.


ACRN Device Model, Service VM, and User VM
******************************************

To keep the hypervisor code base as small and efficient as possible, the
bulk of the device model implementation resides in the Service VM to
provide sharing and other capabilities. The details of which devices are
shared and the mechanism used for their sharing is described in
`pass-through`_ section below.

The Service VM runs with the system's highest virtual machine priority
to ensure required device time-sensitive requirements and system quality
of service (QoS). Service VM tasks run with mixed priority. Upon a
callback servicing a particular User VM request, the corresponding
software (or mediator) in the Service VM inherits the User VM priority.
There may also be additional low-priority background tasks within the
Service OS.

In the automotive example we described above, the User VM is the central
hub of vehicle control and in-vehicle entertainment. It provides support
for radio and entertainment options, control of the vehicle climate
control, and vehicle navigation displays. It also provides connectivity
options for using USB, Bluetooth, and Wi-Fi for third-party device
interaction with the vehicle, such as Android Auto\* or Apple CarPlay*,
and many other features.

Boot Sequence
*************

.. _systemd-boot: https://www.freedesktop.org/software/systemd/man/systemd-boot.html
.. _grub: https://www.gnu.org/software/grub/manual/grub/
.. _Slim Bootloader: https://www.intel.com/content/www/us/en/design/products-and-solutions/technologies/slim-bootloader/overview.html

ACRN supports two kinds of boots: **De-privilege boot mode** and **Direct
boot mode**.

De-privilege boot mode
======================

**De-privilege boot mode** is loaded by ``acrn.efi`` under a UEFI
environment. The Service VM must be the first launched VM, (i.e. VM0).

In :numref:`boot-flow`, we show a verified Boot Sequence with UEFI
on an Intel Architecture platform NUC (see :ref:`hardware`).

.. graphviz:: images/boot-flow.dot
   :name: boot-flow
   :align: center
   :caption: ACRN Hypervisor De-privilege boot mode Flow

The Boot process proceeds as follows:

#. UEFI verifies and boots the ACRN hypervisor and Service VM Bootloader.
#. UEFI (or Service VM Bootloader) verifies and boots the Service VM kernel.
#. The Service VM kernel verifies and loads the ACRN Device Model and the Virtual
   bootloader through ``dm-verity``.
#. The virtual bootloader starts the User-side verified boot process.

.. note::
   To avoid a hardware resources conflict with the ACRN hypervisor, UEFI
   services shall not use IOMMU. In addition, we only currently support the
   UEFI timer with the HPET MSI.

In this boot mode, both the Service and User VM boot options (e.g. Linux
command-line parameters) are configured following the instructions for the EFI
bootloader used by the Operating System (OS).

* In the case of Clear Linux, the EFI bootloader is `systemd-boot`_ and the Linux
  kernel command-line parameters are defined in the ``.conf`` files.

.. note::

   A virtual `Slim Bootloader`_ called ``vSBL``, can also be used to start User VMs. The
   :ref:`acrn-dm_parameters` provides more information on how to boot a
   User VM using ``vSBL``. Note that in this case, the kernel command-line
   parameters are defined by the combination of the ``cmdline.txt`` passed
   on to the ``iasimage`` script and in the launch script, via the ``-B``
   option.

Direct boot mode
================

The ACRN hypervisor can be booted from a third-party bootloader
directly, called **Direct boot mode**. A popular bootloader is `grub`_ and is
also widely used by Linux distributions.

:ref:`using_grub` has a introduction on how to boot ACRN hypervisor with GRUB.

In :numref:`boot-flow-2`, we show the **Direct boot mode** sequence:

.. graphviz:: images/boot-flow-2.dot
  :name: boot-flow-2
  :align: center
  :caption: ACRN Hypervisor Direct boot mode Boot Flow

The Boot process proceeds as follows:

#. UEFI boots GRUB.
#. GRUB boots the ACRN hypervisor and loads the VM kernels as Multi-boot
   modules.
#. The ACRN hypervisor verifies and boots kernels of the Pre-launched VM and
   Service VM.
#. In the Service VM launch path, the Service VM kernel verifies and loads
   the ACRN Device Model and Virtual bootloader through ``dm-verity``.
#. The virtual bootloader starts the User-side verified boot process.

In this boot mode, the boot options of pre-launched VM and service VM are defined
in the variable of ``bootargs`` of struct ``vm_configs[vm id].os_config``
in the source code ``misc/vm_configs/$(SCENARIO)/vm_configurations.c`` by default.
Their boot options can be overridden by the GRUB menu. See :ref:`using_grub` for
details. The boot options of post-launched VM is not covered by hypervisor
source code or GRUB menu, it is defined in guest image file or specified by
launch scripts.

.. note::

   `Slim Bootloader`_ is an alternative boot firmware that can be used to
   boot ACRN in **Direct boot mode**. The `Boot ACRN Hypervisor
   <https://slimbootloader.github.io/how-tos/boot-acrn.html>`_ tutorial
   provides more information on how to use SBL with ACRN.


ACRN Hypervisor Architecture
****************************

ACRN hypervisor is a Type 1 hypervisor, running directly on bare-metal
hardware. It implements a hybrid VMM architecture, using a privileged
service VM, running the Service VM that manages the I/O devices and
provides I/O mediation. Multiple User VMs are supported, with each of
them running different OSs.

Running systems in separate VMs provides isolation between other VMs and
their applications, reducing potential attack surfaces and minimizing
safety interference.  However, running the systems in separate VMs may
introduce additional latency for applications.

:numref:`V2-hl-arch` shows the ACRN hypervisor architecture, with
all types of Virtual Machines (VMs) represented:

- Pre-launched User VM (Safety/RTVM)
- Pre-launched Service VM
- Post-launched User VM
- Kata Container VM (post-launched)
- Real-Time VM (RTVM)

The Service VM owns most of the devices including the platform devices, and
provides I/O mediation. The notable exceptions are the devices assigned to the
pre-launched User VM. Some of the PCIe devices may be passed through
to the post-launched User OSes via the VM configuration. The Service VM runs
hypervisor-specific applications together, such as the ACRN device model, and
ACRN VM manager.

ACRN hypervisor also runs the ACRN VM manager to collect running
information of the User OS, and controls the User VM such as starting,
stopping, and pausing a VM, pausing or resuming a virtual CPU.

.. figure:: images/architecture.png
   :width: 600px
   :align: center
   :name: ACRN-architecture

   ACRN Hypervisor Architecture

ACRN hypervisor takes advantage of Intel Virtualization Technology
(Intel VT), and ACRN hypervisor runs in Virtual Machine Extension (VMX)
root operation, or host mode, or VMM mode. All the guests, including
User VM and Service VM, run in VMX non-root operation, or guest mode. (Hereafter,
we use the terms VMM mode and Guest mode for simplicity).

The VMM mode has 4 protection rings, but runs the ACRN hypervisor in
ring 0 privilege only, leaving rings 1-3 unused. The guest (including
Service VM and User VM), running in Guest mode, also has its own four protection
rings (ring 0 to 3). The User kernel runs in ring 0 of guest mode, and
user land applications run in ring 3 of User mode (ring 1 & 2 are
usually not used by commercial OSes).

.. figure:: images/VMX-brief.png
   :align: center
   :name: VMX-brief

   VMX Brief

As shown in :numref:`VMX-brief`, VMM mode and guest mode are switched
through VM Exit and VM Entry. When the bootloader hands off control to
the ACRN hypervisor, the processor hasn't enabled VMX operation yet. The
ACRN hypervisor needs to enable VMX operation thru a VMXON instruction
first. Initially, the processor stays in VMM mode when the VMX operation
is enabled. It enters guest mode thru a VM resume instruction (or first
time VM launch), and returns back to VMM mode thru a VM exit event. VM
exit occurs in response to certain instructions and events.

The behavior of processor execution in guest mode is controlled by a
virtual machine control structure (VMCS). VMCS contains the guest state
(loaded at VM Entry, and saved at VM Exit), the host state, (loaded at
the time of VM exit), and the guest execution controls. ACRN hypervisor
creates a VMCS data structure for each virtual CPU, and uses the VMCS to
configure the behavior of the processor running in guest mode.

When the execution of the guest hits a sensitive instruction, a VM exit
event may happen as defined in the VMCS configuration. Control goes back
to the ACRN hypervisor when the VM exit happens. The ACRN hypervisor
emulates the guest instruction (if the exit was due to privilege issue)
and resumes the guest to its next instruction, or fixes the VM exit
reason (for example if a guest memory page is not mapped yet) and resume
the guest to re-execute the instruction.

Note that the address space used in VMM mode is different from that in
guest mode. The guest mode and VMM mode use different memory mapping
tables, and therefore the ACRN hypervisor is protected from guest
access. The ACRN hypervisor uses EPT to map the guest address, using the
guest page table to map from guest linear address to guest physical
address, and using the EPT table to map from guest physical address to
machine physical address or host physical address (HPA).

ACRN Device Model Architecture
******************************

Because devices may need to be shared between VMs, device emulation is
used to give VM applications (and OSes) access to these shared devices.
Traditionally there are three architectural approaches to device
emulation:

* The first architecture is **device emulation within the hypervisor** which
  is a common method implemented within the VMware\* workstation product
  (an operating system-based hypervisor). In this method, the hypervisor
  includes emulations of common devices that the various guest operating
  systems can share, including virtual disks, virtual network adapters,
  and other necessary platform elements.

* The second architecture is called **user space device emulation**. As the
  name implies, rather than the device emulation being embedded within
  the hypervisor, it is instead implemented in a separate user space
  application. QEMU, for example, provides this kind of device emulation
  also used by a large number of independent hypervisors. This model is
  advantageous, because the device emulation is independent of the
  hypervisor and can therefore be shared for other hypervisors. It also
  permits arbitrary device emulation without having to burden the
  hypervisor (which operates in a privileged state) with this
  functionality.

* The third variation on hypervisor-based device emulation is
  **paravirtualized (PV) drivers**. In this model introduced by the `XEN
  project`_ the hypervisor includes the physical drivers, and each guest
  operating system includes a hypervisor-aware driver that works in
  concert with the hypervisor drivers.

.. _XEN project:
   https://wiki.xenproject.org/wiki/Understanding_the_Virtualization_Spectrum

In the device emulation models discussed above, there's a price to pay
for sharing devices. Whether device emulation is performed in the
hypervisor, or in user space within an independent VM, overhead exists.
This overhead is worthwhile as long as the devices need to be shared by
multiple guest operating systems. If sharing is not necessary, then
there are more efficient methods for accessing devices, for example
"passthrough".

ACRN device model is a placeholder of the User VM. It allocates memory for
the User OS, configures and initializes the devices used by the User VM,
loads the virtual firmware, initializes the virtual CPU state, and
invokes the ACRN hypervisor service to execute the guest instructions.
ACRN Device model is an application running in the Service VM that
emulates devices based on command line configuration, as shown in
the architecture diagram :numref:`device-model` below:

.. figure:: images/device-model.png
   :align: center
   :name: device-model

   ACRN Device Model

ACRN Device model incorporates these three aspects:

**Device Emulation**:
  ACRN Device model provides device emulation routines that register
  their I/O handlers to the I/O dispatcher. When there is an I/O request
  from the User VM device, the I/O dispatcher sends this request to the
  corresponding device emulation routine.

**I/O Path**:
  see `ACRN-io-mediator`_ below

**VHM**:
  The Virtio and Hypervisor Service Module is a kernel module in the
  Service VM acting as a middle layer to support the device model. The VHM
  and its client handling flow is described below:

  #. ACRN hypervisor IOREQ is forwarded to the VHM by an upcall
     notification to the Service VM.
  #. VHM will mark the IOREQ as "in process" so that the same IOREQ will
     not pick up again. The IOREQ will be sent to the client for handling.
     Meanwhile, the VHM is ready for another IOREQ.
  #. IOREQ clients are either an Service VM Userland application or a Service VM
     Kernel space module. Once the IOREQ is processed and completed, the
     Client will issue an IOCTL call to the VHM to notify an IOREQ state
     change. The VHM then checks and hypercalls to ACRN hypervisor
     notifying it that the IOREQ has completed.

.. note::
   * Userland: dm as ACRN Device Model.
   * Kernel space: VBS-K, MPT Service, VHM itself

.. _pass-through:

Device passthrough
******************

At the highest level, device passthrough is about providing isolation
of a device to a given guest operating system so that the device can be
used exclusively by that guest.

.. figure:: images/device-passthrough.png
   :align: center
   :name: device-passthrough

   Device Passthrough

Near-native performance can be achieved by using device passthrough.
This is ideal for networking applications (or those with high disk I/O
needs) that have not adopted virtualization because of contention and
performance degradation through the hypervisor (using a driver in the
hypervisor or through the hypervisor to a user space emulation).
Assigning devices to specific guests is also useful when those devices
inherently wouldn't be shared. For example, if a system includes
multiple video adapters, those adapters could be passed through to
unique guest domains.

Finally, there may be specialized PCI devices that only one guest domain
uses, so they should be passed through to the guest. Individual USB
ports could be isolated to a given domain too, or a serial port (which
is itself not shareable) could be isolated to a particular guest. In
ACRN hypervisor, we support USB controller passthrough only and we
don't support passthrough for a legacy serial port, (for example
0x3f8).


Hardware support for device passthrough
=======================================

Intel's current processor architectures provides support for device
passthrough with VT-d. VT-d maps guest physical address to machine
physical address, so device can use guest physical address directly.
When this mapping occurs, the hardware takes care of access (and
protection), and the guest operating system can use the device as if it
were a non-virtualized system. In addition to mapping guest to physical
memory, isolation prevents this device from accessing memory belonging
to other guests or the hypervisor.

Another innovation that helps interrupts scale to large numbers of VMs
is called Message Signaled Interrupts (MSI). Rather than relying on
physical interrupt pins to be associated with a guest, MSI transforms
interrupts into messages that are more easily virtualized (scaling to
thousands of individual interrupts). MSI has been available since PCI
version 2.2 but is also available in PCI Express (PCIe), where it allows
fabrics to scale to many devices. MSI is ideal for I/O virtualization,
as it allows isolation of interrupt sources (as opposed to physical pins
that must be multiplexed or routed through software).

Hypervisor support for device passthrough
=========================================

By using the latest virtualization-enhanced processor architectures,
hypervisors and virtualization solutions can support device
passthrough (using VT-d), including Xen, KVM, and ACRN hypervisor.
In most cases, the guest operating system (User
OS) must be compiled to support passthrough, by using
kernel build-time options. Hiding the devices from the host VM may also
be required (as is done with Xen using pciback). Some restrictions apply
in PCI, for example, PCI devices behind a PCIe-to-PCI bridge must be
assigned to the same guest OS. PCIe does not have this restriction.

.. _ACRN-io-mediator:

ACRN I/O mediator
*****************

:numref:`io-emulation-path` shows the flow of an example I/O emulation path.

.. figure:: images/io-emulation-path.png
   :align: center
   :name: io-emulation-path

   I/O Emulation Path

Following along with the numbered items in :numref:`io-emulation-path`:

1. When a guest execute an I/O instruction (PIO or MMIO), a VM exit happens.
   ACRN hypervisor takes control, and analyzes the the VM
   exit reason, which is a VMX_EXIT_REASON_IO_INSTRUCTION for PIO access.
2. ACRN hypervisor fetches and analyzes the guest instruction, and
   notices it is a PIO instruction (``in AL, 20h`` in this example), and put
   the decoded information (including the PIO address, size of access,
   read/write, and target register) into the shared page, and
   notify/interrupt the Service VM to process.
3. The Virtio and hypervisor service module (VHM) in Service VM receives the
   interrupt, and queries the IO request ring to get the PIO instruction
   details.
4. It checks to see if any kernel device claims
   ownership of the IO port: if a kernel module claimed it, the kernel
   module is activated to execute its processing APIs. Otherwise, the VHM
   module leaves the IO request in the shared page and wakes up the
   device model thread to process.
5. The ACRN device model follow the same mechanism as the VHM. The I/O
   processing thread of device model queries the IO request ring to get the
   PIO instruction details and checks to see if any (guest) device emulation
   module claims ownership of the IO port: if a module claimed it,
   the module is invoked to execute its processing APIs.
6. After the ACRN device module completes the emulation (port IO 20h access
   in this example), (say uDev1 here), uDev1 puts the result into the
   shared page (in register AL in this example).
7. ACRN device model then returns control to ACRN hypervisor to indicate the
   completion of an IO instruction emulation, typically thru VHM/hypercall.
8. The ACRN hypervisor then knows IO emulation is complete, and copies
   the result to the guest register context.
9. The ACRN hypervisor finally advances the guest IP to
   indicate completion of instruction execution, and resumes the guest.

The MMIO path is very similar, except the VM exit reason is different. MMIO
access usually is trapped thru VMX_EXIT_REASON_EPT_VIOLATION in
the hypervisor.

Virtio framework architecture
*****************************

.. _Virtio spec:
   http://docs.oasis-open.org/virtio/virtio/v1.0/virtio-v1.0.html

Virtio is an abstraction for a set of common emulated devices in any
type of hypervisor. In the ACRN reference stack, our
implementation is compatible with `Virtio spec`_ 0.9 and 1.0. By
following this spec, virtual environments and guests
should have a straightforward, efficient, standard and extensible
mechanism for virtual devices, rather than boutique per-environment or
per-OS mechanisms.

Virtio provides a common frontend driver framework which not only
standardizes device interfaces, but also increases code reuse across
different virtualization platforms.

.. figure:: images/virtio-architecture.png
   :width: 500px
   :align: center
   :name: virtio-architecture

   Virtio Architecture

To better understand Virtio, especially its usage in
the ACRN project, several key concepts of Virtio are highlighted
here:

**Front-End Virtio driver** (a.k.a. frontend driver, or FE driver in this document)
  Virtio adopts a frontend-backend architecture, which enables a simple
  but flexible framework for both frontend and backend Virtio driver. The
  FE driver provides APIs to configure the interface, pass messages, produce
  requests, and notify backend Virtio driver. As a result, the FE driver
  is easy to implement and the performance overhead of emulating device is
  eliminated.

**Back-End Virtio driver** (a.k.a. backend driver, or BE driver in this document)
  Similar to FE driver, the BE driver, runs either in user-land or
  kernel-land of host OS. The BE driver consumes requests from FE driver
  and send them to the host's native device driver. Once the requests are
  done by the host native device driver, the BE driver notifies the FE
  driver about the completeness of the requests.

**Straightforward**: Virtio devices as standard devices on existing Buses
  Instead of creating new device buses from scratch, Virtio devices are
  built on existing buses. This gives a straightforward way for both FE
  and BE drivers to interact with each other. For example, FE driver could
  read/write registers of the device, and the virtual device could
  interrupt FE driver, on behalf of the BE driver, in case of something is
  happening.  Currently Virtio supports PCI/PCIe bus and MMIO bus. In
  ACRN project, only PCI/PCIe bus is supported, and all the Virtio devices
  share the same vendor ID 0x1AF4.

**Efficient**: batching operation is encouraged
  Batching operation and deferred notification are important to achieve
  high-performance I/O, since notification between FE and BE driver
  usually involves an expensive exit of the guest. Therefore batching
  operating and notification suppression are highly encouraged if
  possible. This will give an efficient implementation for the performance
  critical devices.

**Standard: virtqueue**
  All the Virtio devices share a standard ring buffer and descriptor
  mechanism, called a virtqueue, shown in Figure 6. A virtqueue
  is a queue of scatter-gather buffers. There are three important
  methods on virtqueues:

  * ``add_buf`` is for adding a request/response buffer in a virtqueue
  * ``get_buf`` is for getting a response/request in a virtqueue, and
  * ``kick`` is for notifying the other side for a virtqueue to
    consume buffers.

  The virtqueues are created in guest physical memory by the FE drivers.
  The BE drivers only need to parse the virtqueue structures to obtain
  the requests and get the requests done. How virtqueue is organized is
  specific to the User OS. In the implementation of Virtio in Linux, the
  virtqueue is implemented as a ring buffer structure called vring.

  In ACRN, the virtqueue APIs can be leveraged
  directly so users don't need to worry about the details of the
  virtqueue. Refer to the User VM for
  more details about the virtqueue implementations.

**Extensible: feature bits**
  A simple extensible feature negotiation mechanism exists for each virtual
  device and its driver. Each virtual device could claim its
  device specific features while the corresponding driver could respond to
  the device with the subset of features the driver understands. The
  feature mechanism enables forward and backward compatibility for the
  virtual device and driver.

In the ACRN reference stack, we implement user-land and kernel
space as shown in :numref:`virtio-framework-userland`:

.. figure:: images/virtio-framework-userland.png
   :width: 600px
   :align: center
   :name: virtio-framework-userland

   Virtio Framework - User Land

In the Virtio user-land framework, the implementation is compatible with
Virtio Spec 0.9/1.0. The VBS-U is statically linked with Device Model,
and communicates with Device Model through the PCIe interface: PIO/MMIO
or MSI/MSIx. VBS-U accesses Virtio APIs through user space vring service
API helpers. User space vring service API helpers access shared ring
through remote memory map (mmap). VHM maps User VM memory with the help of
ACRN Hypervisor.

.. figure:: images/virtio-framework-kernel.png
   :width: 600px
   :align: center
   :name: virtio-framework-kernel

   Virtio Framework - Kernel Space

VBS-U offloads data plane processing to VBS-K. VBS-U initializes VBS-K
at the right timings, for example. The FE driver sets
VIRTIO_CONFIG_S_DRIVER_OK to avoid unnecessary device configuration
changes while running. VBS-K can access shared rings through VBS-K
virtqueue APIs. VBS-K virtqueue APIs are similar to VBS-U virtqueue
APIs. VBS-K registers as VHM client(s) to handle a continuous range of
registers

There may be one or more VHM-clients for each VBS-K, and there can be a
single VHM-client for all VBS-Ks as well. VBS-K notifies FE through VHM
interrupt APIs.

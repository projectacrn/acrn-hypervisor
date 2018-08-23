:orphan:

.. _glossary:

Glossary of Terms
#################

.. glossary::
   :sorted:

   ACPI
      Advanced Configuration and Power Interface

   ACRN
      ACRN is a flexible, lightweight reference hypervisor, built with
      real-time and safety-criticality in mind, optimized to streamline
      embedded development through an open source platform.

   AcrnGT
      Intel GVT-g technology for ACRN.

   ACRN-DM
      A user mode device model application running in Service OS to provide
      device emulations in ACRN hypervisor.

   aperture, Low GM
      CPU-visible graphics memory

   API
      Application Program Interface: A defined set of routines and protocols for
      building application software.

   APL
      Apollo Lake platform

   BDW
      Broadwell, Intel 5th-generation CPU platform

   BIOS
      Basic Input/Output System.

   Dom0 i915
      The Intel Graphics driver running in Domain 0

   ELSP
      GPU's ExecList submission port

   GGTT
      Global Graphic Translation Table.  The virtual address page table
      used by a GPU to reference system memory.

   GMA
      Graphics Memory Address

   GPU
      Graphics Processing Unit

   GTT
      Graphic Translation Table

   GTTMMADR
      Graphic Translation Table Memory Map Address

   GuC
      Graphic Micro-controller

   GVT
      Graphics Virtual Technology. GVT-g core device model module up-streamed
      to the Linux kernel.

   GVT-d
      Virtual dedicated graphics acceleration (one VM to one physical GPU)

   GVT-g
      Virtual graphics processing unit (multiple VMs to one physical GPU)

   GVT-s
      Virtual shared graphics acceleration (multiple VMs to one physical GPU)

   Hidden GM, High GM
      Hidden or High graphics memory, not visible to the CPU.

   I2C
      Inter-Integrated Circuit

   i915
      The Intel Graphics driver

   IC
      Instrument Cluster

   IDT
      Interrupt Descriptor Table: a data structure used by the x86
      architecture to implement an interrupt vector table. The IDT is used
      to determine the correct response to interrupts and exceptions.

   ISR
      Interrupt Service Routine: Also known as an interrupt handler, an ISR
      is a callback function whose execution is triggered by a hardware
      interrupt (or software interrupt instructions) and is used to handle
      high-priority conditions that require interrupting the current code
      executing on the processor.

   IVE
      In-Vehicle Experience

   IVI
      In-vehicle Infotainment

   OS
      Operating System

   OSPM
      Operating System Power Management

   Pass-Through Devices
      Physical devices (typically PCI) exclusively assigned to a guest.  In
      the Project ACRN architecture, pass-through devices are owned by the
      foreground OS.

   PCI
      Peripheral Component Interface.

   PDE
      Page Directory Entry

   PM
      Power Management

   PTE
      Page Table Entry

   PV
      Para-virtualization (See
      https://en.wikipedia.org/wiki/Paravirtualization)

   PVINFO
      Para-Virtualization Information Page, a MMIO range used to
      implement para-virtualization

   QEMU
      Quick EMUlator.  Machine emulator running in user space.

   RSE
      Rear Seat Entertainment

   SDC
      Software Defined Cockpit

   SOS
      Service OS, the privileged guest for ACRN hypervisor

   UEFI
      Unified Extensible Firmare Interface. UEFI replaces the
      traditional BIOS on PCs, while also providing BIOS emulation for
      backward compatibility. UEFI can run in 32-bit or 64-bit mode and, more
      important, support Secure Boot, checking the OS validity to ensure no
      malware has tampered with the boot process.

   UOS
      User OS (also known as Guest OS), the unprivileged guest for ACRN
      hypervisor

   vGPU
      Virtual GPU Instance, created by GVT-g and used by a VM

   VHM
      Virtio and Hypervisor Service Module

   Virtio-BE
      Back-End, VirtIO framework provides front-end driver and back-end driver
      for IO mediators, developer has habit of using Shorthand. So they say
      Virtio-BE and Virtio-FE

   Virtio-FE
      Front-End, VirtIO framework provides front-end driver and back-end
      driver for IO mediators, developer has  habit of using Shorthand. So
      they say Virtio-BE and Virtio-FE

   VM
      Virtual Machine, a guest OS running environment

   VMM
      Virtual Machine Monitor

   VMX
      Virtual Machine Extension

   VT
      Intel Virtualization Technology

   VT-d
      Virtualization Technology for Directed I/O

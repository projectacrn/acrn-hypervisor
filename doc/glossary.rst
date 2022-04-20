.. _glossary:

Glossary of Terms
#################

.. glossary::
   :sorted:

   LaaG
   WaaG
      Acronyms for Linux and Windows as a Guest VM. ACRN supports a
      variety of :term:`User VM` OS choices. Your choice depends on the
      needs of your application.  For example, Windows is popular for
      Human-Machine Interface (HMI) applications in industrial applications,
      while Linux is a likely OS choice for a VM running an AI application.

   ACPI
      Advanced Configuration and Power Interface

   ACRN
      ACRN is a flexible, lightweight reference hypervisor, built with
      real-time and safety-criticality in mind, optimized to streamline
      embedded development through an open source platform.

   API
      Application Program Interface: A defined set of routines and protocols for
      building application software.

   APL
      Apollo Lake platform

   BDW
      Broadwell, Intel 5th-generation CPU platform

   BIOS
      Basic Input/Output System.

   DM
   Device Model
      An application within the Service VM responsible for creating and
      launching a User VM and then performing device emulation for the devices
      configured for sharing with that User VM. The Service VM and Device Model
      can access hardware resources directly through native drivers and provide
      device sharing services to User VMs. User VMs can access hardware devices
      directly if they've been configured as passthrough devices.

   Development Computer
   Host
      As with most IoT development environments, you configure, compile, and
      build your application on a separate system from where the application is
      deployed and run (i.e., the :term:`Target`). ACRN recommends using Ubuntu
      18.04 as the OS on your development computer and that is an assumption in
      our documentation.

   Guest
   Guest VM
      A term used to refer to any :term:`VM` that runs on the hypervisor.  Both Service
      and User VMs are considered Guest VMs from the hypervisor's perspective,
      albeit with different properties. *(You'll find the term Guest used in the
      names of functions and variables in the ACRN source code.)*

   GVT-d
      Virtual dedicated graphics acceleration (one VM to one physical GPU).

   Hybrid
      One of three operation scenarios (partitioned, shared, and hybrid) that ACRN supports.
      In the hybrid mode, some physical hardware resources can be  partitioned to
      individual User VMs while others are shared across User VMs.

   IDT
      Interrupt Descriptor Table: a data structure used by the x86
      architecture to implement an interrupt vector table. The IDT is used
      to determine the correct response to interrupts and exceptions.

   ISR
      Interrupt Service Routine: Also known as an interrupt handler, an ISR
      is a callback function whose execution is triggered by a hardware
      interrupt (or software interrupt instructions) and is used to handle
      high-priority conditions that require interrupting the code that is
      executing on the processor.

   Passthrough Device
      Physical I/O devices (typically PCI) exclusively assigned to a User VM so
      that the VM can access the hardware device directly and with minimal (if any)
      VM management involvement. Normally, the Service VM owns the hardware
      devices shared among User VMs and virtualized access is done through
      Device Model emulation.

   Partitioned
      One of three operation scenarios (partitioned, shared, and hybrid) that ACRN supports.
      Physical hardware resources are dedicated to individual User VMs.

   Pre-launched VM
      A :term:`User VM` launched by the hypervisor before the :term:`Service VM`
      is started.  Such a User VM runs independently of and is partitioned from
      the Service VM and other post-launched VMs. It has its own carefully
      configured and dedicated hardware resources such as CPUs, memory, and I/O
      devices. Other VMs, including the Service VM, may not even be aware of a
      pre-launched VM's existence. A pre-launched VM can be used as a
      special-case :term:`Safety VM` for reacting to critical system failures.
      It cannot take advantage of the Service VM or Device Model services.

   Post-launched VM
      A :term:`User VM` configured and launched by the Service VM and typically
      accessing shared hardware resources managed by the Service VM and Device
      Model. Most User VMs are post-launched while special-purpose User VMs are
      pre-launched.

   QEMU
      Quick EMUlator.  Machine emulator running in user space.

   RDT
      Intel Resource Director Technology (Intel RDT) provides a set of
      monitoring and allocation capabilities to control resources such as
      Cache and Memory. ACRN supports Cache Allocation Technology (CAT) and
      Memory Bandwidth Allocation (MBA).

   RTVM
   Real-time VM
      A :term:`User VM` configured specifically for real-time applications and
      their performance needs. ACRN supports near bare-metal performance for a
      post-launched real-time VM by configuring certain key technologies or
      enabling device-passthrough to avoid common virtualization and
      device-access overhead issues. Such technologies include: using a
      passthrough interrupt controller, polling-mode Virtio, Intel RDT
      allocation features (CAT, MBA), and I/O prioritization.  RTVMs are
      typically a :term:`Pre-launched VM`.  A non-:term:`Safety VM` with
      real-time requirements is a :term:`Post-launched VM`.

   Safety VM
      A special VM with dedicated hardware resources for providing overall
      system health-monitoring functionality.  A safety VM is always a
      pre-launched User VM, either in a partitioned or hybrid scenario.

   Scenario
      A collection of hypervisor and VM configuration settings that define an
      ACRN-based application's environment. A scenario configuration is stored
      in a scenario XML file and edited using the ACRN Configurator tool. The
      scenario configuration, along with the target board configuration, is used
      by the ACRN build system to modify the source code to build tailored
      images of the hypervisor and Service VM for the application. ACRN provides
      example scenarios for shared, partitioned, and hybrid configurations that
      developers can use to define a scenario configuration appropriate for
      their own application.

   Service VM
      A special VM, directly launched by the hypervisor. The Service VM can
      access hardware resources directly by running native drivers and provides
      device sharing services to post-launched User VMs through the ACRN Device
      Model (DM). Hardware resources include CPUs, memory, graphics memory, USB
      devices, disk, and network mediation. *(Historically, the Service VM was
      called the Service OS or SOS.)*

   Shared
      One of three operation scenarios (shared, hybrid, partitioned) that ACRN supports.
      Most of the physical hardware resources are shared across User VMs.
      *(In releases prior to 2.7, this was called the "Industry" scenario.)*

   Target
      This is the hardware where the configured ACRN hypervisor and
      developer-written application (built on the :term:`Development Computer`) is
      deployed and runs.

   UEFI
      Unified Extensible Firmare Interface. UEFI replaces the
      traditional BIOS on PCs, while also providing BIOS emulation for
      backward compatibility. UEFI can run in 32-bit or 64-bit mode and, more
      important, support Secure Boot, checking the OS validity to ensure no
      malware has tampered with the boot process.

   User VM
      A :term:`VM` where user-defined environments and applications run. User VMs can
      run different OSes based on their needs, including for example, Ubuntu for
      an AI application, Windows for a Human-Machine Interface, or a
      hard real-time control OS such as Zephyr, VxWorks, or RT-Linux for soft or
      hard real-time control. There are three types of ACRN User VMs: pre-launched,
      post-launched standard, and post-launched real-time.  *(Historically, a
      User VM was also called a User OS, or simply UOS.)*

   Virtual Machine
     A compute resource that uses software instead of physical hardware to run a
     program. Multiple VMs can run independently on the same physical machine,
     and with their own OS. A hypervisor uses direct access to the underlying
     machine to create the software environment for sharing and managing
     hardware resources.

   VMM
      Virtual Machine Monitor

   VMX
      Virtual Machine Extension

   VT
      Intel Virtualization Technology

   VT-d
      Virtualization Technology for Directed I/O

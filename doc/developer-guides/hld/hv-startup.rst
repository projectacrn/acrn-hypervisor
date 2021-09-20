.. _hv-startup:

Hypervisor Startup
##################

This section is an overview of the ACRN hypervisor startup.
The ACRN hypervisor
compiles to a 32-bit multiboot-compliant ELF file.
The bootloader (ABL/SBL or GRUB) loads the hypervisor according to the
addresses specified in the ELF header. The BSP starts the hypervisor
with an initial state compliant to multiboot 1 specification, after the
bootloader prepares full configurations including ACPI, E820, etc.

The HV startup has two parts: the native startup followed by
VM startup.

Multiboot Header
****************

The ACRN hypervisor is built with a multiboot header, which presents
``MULTIBOOT_HEADER_MAGIC`` and ``MULTIBOOT_HEADER_FLAGS`` at the beginning
of the image, and it sets bit 6 in ``MULTIBOOT_HEADER_FLAGS`` which requests
bootloader passing memory mmap information(like e820 entries) through
Multiboot Information(MBI) structure.

Native Startup
**************

.. figure:: images/hld-image107.png
   :align: center
   :name: hvstart-nativeflow

   Hypervisor Native Startup Flow

Native startup sets up a baseline environment for HV, including basic
memory and interrupt initialization as shown in
:numref:`hvstart-nativeflow`. Here is a short
description for the flow:

-  **BSP Startup:** The starting point for bootstrap processor.

-  **Relocation**: Relocate the hypervisor image if the hypervisor image
   is not placed at the assumed base address.

-  **UART Init:** Initialize a pre-configured UART device used
   as the base physical console for HV and Service OS.

-  **Memory Init:** Initialize memory type and cache policy, and creates
   MMU page table mapping for HV.

-  **Scheduler Init:** Initialize scheduler framework, which provide the
   capability to switch different threads(like vcpu vs. idle thread) on a
   physical CPU, and to support CPU sharing.

-  **Interrupt Init:** Initialize interrupt and exception for native HV
   including IDT and ``do_IRQ`` infrastructure; a timer interrupt
   framework is then built. The native/physical interrupts will go
   through this ``do_IRQ`` infrastructure then distribute to special
   targets (HV or VMs).

-  **Start AP:** BSP kicks ``INIT-SIPI-SIPI`` IPI sequence to start other
   native APs (application processor). Each AP will initialize its
   own memory and interrupts, notifies the BSP on completion and
   enter the default idle loop.

-  **Shell Init:** Start a command shell for HV accessible via the UART.

Symbols in the hypervisor are placed with an assumed base address, but
the bootloader may not place the hypervisor at that specified base. In
this case, the hypervisor will relocate itself to where the bootloader
loads it.

Here is a summary of CPU and memory initial states that are set up after
the native startup.

CPU
   ACRN hypervisor brings all physical processors to 64-bit IA32e
   mode, with the assumption that the BSP starts in protection mode where
   segmentation and paging sets an identical mapping of the first 4G
   addresses without permission restrictions. The control registers and
   some MSRs are set as follows:

   -  cr0: The following features are enabled: paging, write protection,
      protection mode, numeric error and co-processor monitoring.

   -  cr3: refer to the initial state of memory.

   -  cr4: The following features are enabled: physical address extension,
      machine-check, FXSAVE/FXRSTOR, SMEP, VMX operation and unmask
      SIMD FP exception. The other features are disabled.

   -  MSR_IA32_EFER: only IA32e mode is enabled.

   -  MSR_IA32_FS_BASE: the address of stack canary, used for detecting
      stack smashing.

   -  MSR_IA32_TSC_AUX: a unique logical ID is set for each physical
      processor.

   -  stack: each physical processor has a separate stack.

Memory
   All physical processors are in 64-bit IA32e mode after
   startup. The GDT holds four entries, one unused, one for code and
   another for data, both of which have a base of all 0's and a limit of
   all 1's, and the other for 64-bit TSS. The TSS only holds three stack
   pointers (for machine-check, double fault and stack fault) in the
   interrupt stack table (IST) which are different across physical
   processors. LDT is disabled.

Refer to :ref:`physical-interrupt-initialization` for a detailed description of interrupt-related
initial states, including IDT and physical PICs.

After the BSP detects that all APs are up, it will continue to enter guest mode; similar, after one AP
complete its initialization, it will start entering guest mode as well.
When BSP & APs enter guest mode, they will try to launch predefined VMs whose vBSP associated with
this physical core; these predefined VMs are static configured in ``vm config`` and they could be
pre-launched Safety VM or Service VM; the VM startup will be explained in next section.

.. _vm-startup:

VM Startup
**********

The Service VM or a pre-launched VM is created and launched on the physical
CPU which configured as its vBSP. Meanwhile, for the physical CPUs which
configured as vAPs for dedicated VMs, they will enter the default idle loop
(refer to :ref:`VCPU_lifecycle` for details), waiting for any vCPU to be
scheduled to them.

:numref:`hvstart-vmflow` illustrates a high-level execution flow of
creating and launching a VM, applicable to pre-launched VM, Service VM
and User VM. One major difference in the creation of User VM and pre-launched
/Service VM is that pre-launched/Service VM is created by the hypervisor,
while the creation of User VMs is triggered by the DM in Service OS.
The main steps include:

-  **Create VM**: A VM structure is allocated and initialized. A unique
   VM ID is picked, EPT is initialized, e820 table for this VM is prepared,
   I/O bitmap is set up, virtual PIC/IOAPIC/PCI/UART is initialized, EPC for
   virtual SGX is prepared, guest PM IO is set up, IOMMU for PT dev support
   is enabled, virtual CPUID entries are filled, and vCPUs configured in this VM's
   ``vm config`` are prepared. For post-launched User VM, the EPT page table and
   e820 table is actually prepared by DM instead of hypervisor.

-  **Prepare vCPUs:** Create the vCPUs, assign the physical processor it
   is pinned to, a unique-per-VM vCPU ID and a globally unique VPID,
   and initializes its virtual lapic and MTRR, and its vCPU thread object got setup
   for vcpu scheduling. The vCPU number and affinity are defined in corresponding
   ``vm config`` for this VM.

-  **Build vACPI:** For the Service VM, the hypervisor will customize a virtual ACPI
   table based on the native ACPI table (this is in the TODO).
   For a pre-launched VM, the hypervisor will build a simple ACPI table with necessary
   information like MADT.
   For a post-launched User VM, the DM will build its ACPI table dynamically.

-  **SW Load:** Prepares for each VM's SW configuration according to guest OS
   requirement, which may include kernel entry address, ramdisk address,
   bootargs, or zero page for launching bzImage etc.
   This is done by the hypervisor for pre-launched or Service VM, and the VM will
   start from the standard real or protected mode which is not related to the
   native environment. For post-launched VMs, the VM's SW configuration is done
   by DM.

-  **Start VM:** The vBSP of vCPUs in this VM is kick to do schedule.

-  **Schedule vCPUs:** The vCPUs are scheduled to the corresponding
   physical processors for execution.

-  **Init VMCS:** Initialize vCPU's VMCS for its host state, guest
   state, execution control, entry control and exit control. It's
   the last configuration before vCPU runs.

-  **vCPU thread:** vCPU kicks out to run. For vBSP of vCPUs, it will
   start running into kernel image which SW Load is configured; for
   any vAP of vCPUs, it will wait for INIT-SIPI-SIPI IPI sequence
   trigger from its vBSP.

.. figure:: images/hld-image104.png
   :align: center
   :name: hvstart-vmflow

   Hypervisor VM Startup Flow

SW configuration for Service VM (bzimage SW load as example):

-  **ACPI**: HV passes the entire ACPI table from bootloader to Service
   VM directly. Legacy mode is currently supported as the ACPI table
   is loaded at F-Segment.

-  **E820**: HV passes e820 table from bootloader through zero-page
   after the HV reserved (32M for example) and pre-launched VM owned
   memory is filtered out.

-  **Zero Page**: HV prepares the zero page at the high end of Service
   VM memory which is determined by SOS_VM guest FIT binary build. The
   zero page includes configuration for ramdisk, bootargs and e820
   entries. The zero page address will be set to vBSP RSI register
   before VCPU gets run.

-  **Entry address**: HV will copy Service OS kernel image to
   kernel_load_addr, which could be got from "pref_addr" field in bzimage
   header; the entry address will be calculated based on kernel_load_addr,
   and will be set to vBSP RIP register before VCPU gets run.

SW configuration for post-launched User VMs (OVMF SW load as example):

-  **ACPI**: the virtual ACPI table is built by DM and put at User VM's
   F-Segment. Refer to :ref:`hld-io-emulation` for details.

-  **E820**: the virtual E820 table is built by the DM then passed to
   the virtual bootloader. Refer to :ref:`hld-io-emulation` for details.

-  **Entry address**: the DM will copy User OS kernel(OVMF) image to
   OVMF_NVSTORAGE_OFFSET - normally is @(4G - 2M), and set the entry
   address to 0xFFFFFFF0. As the vBSP will kick to run virtual bootloader
   (OVMF) from real-mode, so its CS base will be set as 0xFFFF0000, and
   RIP register will be set as 0xFFF0.

SW configuration for pre-launched VMs (raw SW load as example):

-  **ACPI**: the virtual ACPI table is built by the hypervisor and put at
   this VM's F-Segment.

-  **E820**: the virtual E820 table is built by the hypervisor then passed to
   the VM according to different SW loaders. For raw SW load here, it's not
   used.

-  **Entry address**: the hypervisor will copy User OS kernel image to
   kernel_load_addr which set by ``vm config``, and set the entry
   address to kernel_entry_addr which set by ``vm config`` as well.

Here is initial mode of vCPUs:


+----------------------------------+----------------------------------------------------------+
|  VM and Processor Type           |    Initial Mode                                          |
+=================+================+==========================================================+
| Service VM      |        BSP     |   Same as physical BSP, or Real Mode if Service VM boot  |
|                 |                |   w/ OVMF                                                |
|                 +----------------+----------------------------------------------------------+
|                 |        AP      |   Real Mode                                              |
+-----------------+----------------+----------------------------------------------------------+
| User VM         |        BSP     |   Real Mode                                              |
|                 +----------------+----------------------------------------------------------+
|                 |        AP      |   Real Mode                                              |
+-----------------+----------------+----------------------------------------------------------+
| Pre-launched VM |        BSP     |   Real Mode or Protected Mode                            |
|                 +----------------+----------------------------------------------------------+
|                 |        AP      |   Real Mode                                              |
+-----------------+----------------+----------------------------------------------------------+


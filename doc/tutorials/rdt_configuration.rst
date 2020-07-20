.. _rdt_configuration:

Enable RDT Configuration
########################

On x86 platforms that support Intel Resource Director Technology (RDT)
allocation features such as Cache Allocation Technology (CAT) and Memory
Bandwidth Allocation (MBA), the ACRN hypervisor can be used to limit regular
VMs which may be over-utilizing common resources such as cache and memory
bandwidth relative to their priorities so that the performance of other
higher priorities VMs (such as RTVMs) are not impacted.

Using RDT includes three steps:

1. Detect and enumerate RDT allocation capabilities on supported
   resources such as cache and memory bandwidth.
#. Set up resource mask array MSRs (Model-Specific Registers) for each
   CLOS (Class of Service, which is a resource allocation), basically to
   limit or allow access to resource usage.
#. Select the CLOS for the CPU associated with the VM that will apply
   the resource mask on the CP.

Steps #2 and #3 configure RDT resources for a VM and can be done in two ways:

* Using a HV debug shell (See `Tuning RDT resources in HV debug shell`_)
* Using a VM configuration (See `Configure RDT for VM using VM Configuration`_)

The following sections discuss how to detect, enumerate capabilities, and
configure RDT resources for VMs in the ACRN hypervisor.

For further details, refer to the ACRN RDT high-level design
:ref:`hv_rdt` and `Intel 64 and IA-32 Architectures Software Developer's
Manual, (Section 17.19 Intel Resource Director Technology Allocation Features)
<https://software.intel.com/en-us/download/intel-64-and-ia-32-architectures-sdm-combined-volumes-3a-3b-3c-and-3d-system-programming-guide>`_

.. _rdt_detection_capabilities:

RDT detection and resource capabilities
***************************************
From the ACRN HV debug shell, use ``cpuid`` to detect and identify the
resource capabilities. Use the platform's serial port for the HV shell
(refer to :ref:`getting-started-up2` for setup instructions).

Check if the platform supports RDT with ``cpuid``. First, run ``cpuid 0x7 0x0``; the return value ebx [bit 15] is set to 1 if the platform supports
RDT. Next, run ``cpuid 0x10 0x0`` and check the EBX [3-1] bits. EBX [bit 1]
indicates that L3 CAT is supported. EBX [bit 2] indicates that L2 CAT is
supported. EBX [bit 3] indicates that MBA is supported. To query the
capabilities of the supported resources, use the bit position as a subleaf
index. For example, run ``cpuid 0x10 0x2`` to query the L2 CAT capability.

.. code-block:: none

   ACRN:\>cpuid 0x7 0x0
   cpuid leaf: 0x7, subleaf: 0x0, 0x0:0xd39ffffb:0x00000818:0xbc000400

L3/L2 bit encoding:

* EAX [bit 4:0] reports the length of the cache mask minus one. For
  example, a value 0xa means the cache mask is 0x7ff.
* EBX [bit 31:0] reports a bit mask. Each set bit indicates the
  corresponding unit of the cache allocation that can be used by other
  entities in the platform (e.g. integrated graphics engine).
* ECX [bit 2] if set, indicates that cache Code and Data Prioritization
  Technology is supported.
* EDX [bit 15:0] reports the maximum CLOS supported for the resource
  minus one. For example, a value of 0xf means the max CLOS supported
  is 0x10.

  .. code-block:: none

     ACRN:\>cpuid 0x10 0x0
     cpuid leaf: 0x10, subleaf: 0x0, 0x0:0xa:0x0:0x0

     ACRN:\>cpuid 0x10 **0x1**
     cpuid leaf: 0x10, subleaf: 0x1, 0xa:0x600:0x4:0xf

MBA bit encoding:

* EAX [bit 11:0] reports the maximum MBA throttling value minus one. For example, a value 0x59 means the max delay value is 0x60.
* EBX [bit 31:0] reserved.
* ECX [bit 2] reports whether the response of the delay values is linear.
* EDX [bit 15:0] reports the maximum CLOS supported for the resource minus one. For example, a value of 0x7 means the max CLOS supported is 0x8.

  .. code-block:: none

     ACRN:\>cpuid 0x10 0x0
     cpuid leaf: 0x10, subleaf: 0x0, 0x0:0xa:0x0:0x0

     ACRN:\>cpuid 0x10 **0x3**
     cpuid leaf: 0x10, subleaf: 0x3, 0x59:0x0:0x4:0x7

.. note::
   ACRN takes the lowest common CLOS max value between the supported
   resources as maximum supported CLOS ID. For example, if max CLOS
   supported by L3 is 16 and MBA is 8, ACRN programs MAX_PLATFORM_CLOS_NUM
   to 8. ACRN recommends to have consistent capabilities across all RDT
   resources by using a common subset CLOS. This is done in order to minimize
   misconfiguration errors.

Tuning RDT resources in HV debug shell
**************************************
This section explains how to configure the RDT resources from the HV debug
shell.

#. Check the PCPU IDs of each VM; the ``vcpu_list`` below shows that VM0 is
   running on PCPU0, and VM1 is running on PCPU1:

   .. code-block:: none

      ACRN:\>vcpu_list

      VM ID    PCPU ID    VCPU ID    VCPU ROLE    VCPU STATE
      =====    =======    =======    =========    ==========
        0         0          0        PRIMARY       Running
        1         1          0        PRIMARY       Running

#. Set the resource mask array MSRs for each CLOS with a ``wrmsr <reg_num> <value>``.
   For example, if you want to restrict VM1 to use the
   lower 4 ways of LLC cache and you want to allocate the upper 7 ways of
   LLC to access to VM0, you must first assign a CLOS for each VM (e.g. VM0
   is assigned CLOS0 and VM1 CLOS1). Next, resource mask the MSR that
   corresponds to the CLOS0. In our example, IA32_L3_MASK_BASE + 0 is
   programmed to 0x7f0. Finally, resource mask the MSR that corresponds to
   CLOS1. In our example, IA32_L3_MASK_BASE + 1 is set to 0xf.

   .. code-block:: none

      ACRN:\>wrmsr  -p1 0xc90  0x7f0
      ACRN:\>wrmsr  -p1 0xc91  0xf

#. Assign CLOS1 to PCPU1 by programming the MSR IA32_PQR_ASSOC [bit 63:32]
   (0xc8f) to 0x100000000 to use CLOS1 and assign CLOS0 to PCPU 0 by
   programming MSR IA32_PQR_ASSOC [bit 63:32] to 0x0. Note that
   IA32_PQR_ASSOC is per LP MSR and CLOS must be programmed on each LP.

   .. code-block:: none

      ACRN:\>wrmsr   -p0   0xc8f    0x000000000 (this is default and can be skipped)
      ACRN:\>wrmsr   -p1   0xc8f    0x100000000

.. _rdt_vm_configuration:

Configure RDT for VM using VM Configuration
*******************************************

#. RDT hardware feature is enabled by default on supported platforms. This
   information can be found using an offline tool that generates a
   platform-specific xml file that helps ACRN identify RDT-supported
   platforms. RDT on ACRN is enabled by configuring the ``FEATURES``
   sub-section of the scenario xml file as in the below example. For
   details on building ACRN with scenario refer  to :ref:`build-with-acrn-scenario`.

   .. code-block:: none
      :emphasize-lines: 6

      <FEATURES>
         <RELOC desc="Enable hypervisor relocation">y</RELOC>
         <SCHEDULER desc="The CPU scheduler to be used by the hypervisor.">SCHED_BVT</SCHEDULER>
         <MULTIBOOT2 desc="Support boot ACRN from multiboot2 protocol.">y</MULTIBOOT2>
         <RDT desc="Intel RDT (Resource Director Technology).">
            <RDT_ENABLED desc="Enable RDT">*y*</RDT_ENABLED>
            <CDP_ENABLED desc="CDP (Code and Data Prioritization). CDP is an extension of CAT.">n</CDP_ENABLED>
            <CLOS_MASK desc="Cache Capacity Bitmask"></CLOS_MASK>
            <MBA_DELAY desc="Memory Bandwidth Allocation delay value"></MBA_DELAY>
         </RDT>

#. Once RDT is enabled in the scenario xml file, the next step is to program
   the desired cache mask or/and the MBA delay value as needed in the 
   scenario file. Each cache mask or MBA delay configuration corresponds 
   to a CLOS ID. For example, if the maximum supported CLOS ID is 4, then 4 
   cache mask settings needs to be in place where each setting corresponds
   to a CLOS ID starting from 0. To set the cache masks for 4 CLOS ID and 
   use default delay value for MBA, it can be done as shown in the example below.

   .. code-block:: none
      :emphasize-lines: 8,9,10,11,12

      <FEATURES>
         <RELOC desc="Enable hypervisor relocation">y</RELOC>
         <SCHEDULER desc="The CPU scheduler to be used by the hypervisor.">SCHED_BVT</SCHEDULER>
         <MULTIBOOT2 desc="Support boot ACRN from multiboot2 protocol.">y</MULTIBOOT2>
         <RDT desc="Intel RDT (Resource Director Technology).">
            <RDT_ENABLED desc="Enable RDT">y</RDT_ENABLED>
            <CDP_ENABLED desc="CDP (Code and Data Prioritization). CDP is an extension of CAT.">n</CDP_ENABLED>
            <CLOS_MASK desc="Cache Capacity Bitmask">*0xff*</CLOS_MASK>
            <CLOS_MASK desc="Cache Capacity Bitmask">*0x3f*</CLOS_MASK>
            <CLOS_MASK desc="Cache Capacity Bitmask">*0xf*</CLOS_MASK>
            <CLOS_MASK desc="Cache Capacity Bitmask">*0x3*</CLOS_MASK>
            <MBA_DELAY desc="Memory Bandwidth Allocation delay value">*0*</MBA_DELAY>
         </RDT>

   .. note::
      Users can change the mask values, but the cache mask must have
      **continuous bits** or a #GP fault can be triggered. Similary, when
      programming an MBA delay value, be sure to set the value to less than or
      equal to the MAX delay value.

#. Configure each CPU in VMs to a desired CLOS ID in the ``VM`` section of the
   scenario file. Follow `RDT detection and resource capabilities`_
   to identify the maximum supported CLOS ID that can be used. ACRN uses the
   **the lowest common MAX CLOS** value among all RDT resources to avoid
   resource misconfigurations.

   .. code-block:: none
      :emphasize-lines: 5,6,7,8

      <vm id="0">
         <vm_type desc="Specify the VM type" readonly="true">PRE_STD_VM</vm_type>
         <name desc="Specify the VM name which will be shown in hypervisor console command: vm_list.">ACRN PRE-LAUNCHED VM0</name>
         <uuid configurable="0" desc="vm uuid">26c5e0d8-8f8a-47d8-8109-f201ebd61a5e</uuid>
         <clos desc="Class of Service for Cache Allocation Technology. Please refer SDM 17.19.2 for details and use with caution.">
            <vcpu_clos>*0*</vcpu_clos>
            <vcpu_clos>*1*</vcpu_clos>
         </clos>
      </vm>

   .. note::
      In ACRN, Lower CLOS always means higher priority (clos 0 > clos 1 > clos 2> ...clos n).
      So, carefully program each VM's CLOS accordingly.

#. Careful consideration should be made when assigning vCPU affinity. In
   a cache isolation configuration, in addition to isolating CAT-capable
   caches, you must also isolate lower-level caches. In the following
   example, logical processor #0 and #2 share L1 and L2 caches. In this
   case, do not assign LP #0 and LP #2 to different VMs that need to do
   cache isolation. Assign LP #1 and LP #3 with similar consideration:

   .. code-block:: none
      :emphasize-lines: 3

      # lstopo-no-graphics -v
      Package L#0 (P#0 CPUVendor=GenuineIntel CPUFamilyNumber=6 CPUModelNumber=142)
        L3Cache L#0 (size=3072KB linesize=64 ways=12 Inclusive=1)
          L2Cache L#0 (size=256KB linesize=64 ways=4 Inclusive=0)
            L1dCache L#0 (size=32KB linesize=64 ways=8 Inclusive=0)
              L1iCache L#0 (size=32KB linesize=64 ways=8 Inclusive=0)
                Core L#0 (P#0)
                  PU L#0 (P#0)
                  PU L#1 (P#2)
          L2Cache L#1 (size=256KB linesize=64 ways=4 Inclusive=0)
            L1dCache L#1 (size=32KB linesize=64 ways=8 Inclusive=0)
              L1iCache L#1 (size=32KB linesize=64 ways=8 Inclusive=0)
                Core L#1 (P#1)
                  PU L#2 (P#1)
                  PU L#3 (P#3)

#. Bandwidth control is per-core (not per LP), so max delay values of
   per-LP CLOS is applied to the core. If HT is turned on, don't place high
   priority threads on sibling LPs running lower priority threads.

#. Based on our scenario, build the ACRN hypervisor and copy the
   artifact ``acrn.efi`` to the
   ``/boot/EFI/acrn`` directory. If needed, update the devicemodel
   ``acrn-dm`` as well in ``/usr/bin`` directory. see
   :ref:`getting-started-building` for building instructions.

   .. code-block:: none

      $ make hypervisor BOARD=apl-up2 FIRMWARE=uefi
      ...

      # these operations are done on UP2 board
      $ mount /dev/mmcblk0p0 /boot
      $ scp <acrn.efi-at-your-compile-PC> /boot/EFI/acrn

#. Restart the platform.

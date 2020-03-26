.. _rdt_configuration:

RDT Configuration
#################

On x86 platforms that support Intel Resource Director Technology(RDT) Allocation features such as Cache Allocation Technology (CAT) and Memory Bandwidth Allocation(MBA), ACRN hypervisor can be used to limit regular VMs which may be over-utilizing common resources such cache and memory bandwidth relative to their priority so that performance of other higher priority VMs (such as RTVMs) are not impacted.
There are basically 3 steps to use RDT,

1. Detect and enumerate RDT allocation capabilites on supported resources such as cache and memory bandwidth.
2. Setup resource mask array MSRs for each CLOS (Class of Service), basically to limit or allow access to resource usage.
3. Select CLOS for the CPU associated with VM, that will apply the resource mask on the CP

Steps #2 and #3 configure RDT resources for a VM and can be done in 2 ways,
a) Using HV debug shell
b) Using VM configuration

The following sections discuss how to detect, enumerate capabilities and configure RDT resources for VMs in ACRN hypervisor.

For futher details, please refer to ACRN RDT high-level design :ref:`hv_rdt` and `Intel (R) 64 and IA-32 Architectures Software Developer's Manual, (Section 17.19 INTEL® RESOURCE DIRECTOR TECHNOLOGY ALLOCATION FEATURES) <https://software.intel.com/en-us/download/intel-64-and-ia-32-architectures-sdm-combined-volumes-3a-3b-3c-and-3d-system-programming-guide>`_

.. _rdt_detection_capabilities:

RDT detection and resource capabilites
**************************************
From the ACRN HV debug shell, you can use ``cpuid`` to detect and identify the resource capabilities.  You can use the platform's serial port for the HV shell (refer to :ref:`getting-started-up2` for setup instructions).

Check if the platform supports RDT with ``cpuid``. First run ``cpuid 0x7 0x0``, the return value ebx[bit 15] should be set to 1 if the platform supports RDT. Then run ``cpuid 0x10 0x0``, and check EBX[3-1] bits. EBX[bit 1] set, represents L3 CAT is supported, EBX[bit 2] set, represents L2 CAT is supported and EBX[bit 3] set, represents MBA is supported. To query the capabilties of the supported resources, use the bit posistion as subleaf index. For example, run ``cpuid 0x10 0x2`` to query L2 CAT capability.

    .. code-block:: none

      ACRN:\>cpuid 0x7 0x0
      cpuid leaf: 0x7, subleaf: 0x0, 0x0:0xd39ffffb:0x00000818:0xbc000400

   For L3/L2, the following are the bit encoding,
    * EAX[bit 4:0] reports the length of cache mask minus one. For example a value 0xa, means cache mask is 0x7ff.
    * EBX[bit 31:0] reports a bit mask. Each set bit indicates the corresponding unit of the cache allocation may be used by other entities in the platform. (e.g. integrated graphics engine)
    * ECX[bit 2] if set, indicates cache Code and Data Prioritization Technology is supported.
    * EDX[bit 15:0] reports the maximum CLOS supported for the resource minus one.  For example a value of 0xf, means the max CLOS supported is 0x10.

    .. code-block:: none

      ACRN:\>cpuid 0x10 0x0
      cpuid leaf: 0x10, subleaf: 0x0, 0x0:0xa:0x0:0x0

      ACRN:\>cpuid 0x10 **0x1**
      cpuid leaf: 0x10, subleaf: 0x1, 0xa:0x600:0x4:0xf

   For MBA, the following are the bit encoding,
    * EAX[bit 11:0] reports the maximum MBA throttling value minus one. For example a value 0x59, means max delay value is 0x60.
    * EBX[bit 31:0] reserved
    * ECX[bit 2] reports whether the response of the delay values is linear.
    * EDX[bit 15:0] reports the maximum CLOS supported for the resource minus one.  For example a value of 0x7, means the max CLOS supported is 0x8.

    .. code-block:: none

      ACRN:\>cpuid 0x10 0x0
      cpuid leaf: 0x10, subleaf: 0x0, 0x0:0xa:0x0:0x0

      ACRN:\>cpuid 0x10 **0x3**
      cpuid leaf: 0x10, subleaf: 0x3, 0x59:0x0:0x4:0x7


Tuning RDT resources in HV debug shell
**************************************
This section explains how to configure the RDT resources from HV debug shell.

#. Check PCPU IDs of each VM, the ``vcpu_list`` shows that VM0 is running on PCPU0,
   and VM1 is running on PCPU1:

   .. code-block:: none

      ACRN:\>vcpu_list

      VM ID    PCPU ID    VCPU ID    VCPU ROLE    VCPU STATE
      =====    =======    =======    =========    ==========
        0         0          0        PRIMARY       Running
        1         1          0        PRIMARY       Running

#. Set resource mask array MSRs for each CLOS with ``wrmsr <reg_num> <value>``. For example if we want to restrict VM1 to use the lower 4 ways of LLC cache and allocate upper 7 ways of LLC access to VM0, first a CLOS is assigned for each VM. (say VM0 is assigned CLOS 0 and VM1 CLOS1). Then resource mask MSR corresponding to the CLOS0, in this case IA32_L3_MASK_BASE + 0 is programmed to 0x7f0 and resouce mask MSR corresponding to CLOS1, IA32_L3_MASK_BASE + 1 is set to 0xf.

   .. code-block:: none

      ACRN:\>wrmsr  -p1 0xc90  0x7f0
      ACRN:\>wrmsr  -p1 0xc91  0xf

#. Assign CLOS1 to PCPU1 by programming the MSR IA32_PQR_ASSOC [bit 63:32] (0xc8f) to 0x100000000 to use CLOS1 and assign CLOS0 to PCPU 0 by programming MSR IA32_PQR_ASSOC [bit 63:32] to 0x0. Note that, IA32_PQR_ASSOC is per LP MSR and CLOS has to programmed on each LP.

   .. code-block:: none

      ACRN:\>wrmsr   -p0   0xc8f    0x000000000 (this is default and can be skipped)
      ACRN:\>wrmsr   -p1   0xc8f    0x100000000

.. _rdt_vm_configuration:

Configure RDT for VM using VM Configuration
*******************************************

#. RDT on ACRN is enabled by default on platforms that have the support. Thanks to offline tool approach which generates a platform specific xml file using which ACRN identifies if RDT is supported on the platform or not. But the feature can be also be toggled using CONFIG_RDT_ENABLED flag with ``make menuconfig`` command. The first step is to clone the ACRN source code (if you haven't done it already):

   .. code-block:: none

      $ git clone https://github.com/projectacrn/acrn-hypervisor.git
      $ cd acrn-hypervisor/

   .. figure:: images/menuconfig-rdt.png
      :align: center

#. The predefined cache masks can be found at ``hypervisor/arch/x86/configs/$(CONFIG_BOARD)/board.c``, for respective board. For example for apl-up2, it can found at ``hypervisor/arch/x86/configs/apl-up2/board.c``.

   .. code-block:: none
      :emphasize-lines: 3,7,11,15

      struct platform_clos_info platform_l2_clos_array[MAX_PLATFORM_CLOS_NUM] = {
              {
                      .clos_mask = 0xff,
                      .msr_index = MSR_IA32_L3_MASK_BASE + 0,
              },
              {
                      .clos_mask = 0xff,
                      .msr_index = MSR_IA32_L3_MASK_BASE + 1,
              },
              {
                      .clos_mask = 0xff,
                      .msr_index = MSR_IA32_L3_MASK_BASE + 2,
              },
              {
                      .clos_mask = 0xff,
                      .msr_index = MSR_IA32_L3_MASK_BASE + 3,
              },
      };

.. note::
   User can change the mask values, but cache mask must have **continuous bits**, or a #GP fault can be triggered. Similary when programming MBA delay value, care should taken to set the value to less than or equal to MAX dealy value.

#. Set up CLOS in the VM config. Please follow `RDT detection and resource capabilites` to identify the MAX CLOS that can be used. In ACRN we a value between 0 to **the lowest common MAX CLOS** among all the RDT resources to avoid resource misconfigurations. We will take SOS on sharing mode as an example. Its configuration data can be found at ``hypervisor/arch/x86/configs/vm_config.c``

   .. code-block:: none
      :emphasize-lines: 6

      struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] __aligned(PAGE_SIZE) = {
              {
                      .type = SOS_VM,
                      .name = SOS_VM_CONFIG_NAME,
                      .guest_flags = 0UL,
                      .clos = 1,
                      .memory = {
                              .start_hpa = 0x0UL,
                              .size = CONFIG_SOS_RAM_SIZE,
                      },
                      .os_config = {
                              .name = SOS_VM_CONFIG_OS_NAME,
                      },
              },
      };

.. note::
   In ACRN, Lower CLOS always means higher priority (clos 0 > clos 1 > clos 2>...clos n). So care should be taken to program the VMs CLOS accordingly.

#. Careful consideration should be made in assigning vCPU affinity. In cache isolation configuration, not only need to isolate CAT-capable caches, but need to isolate lower-level caches as well. In the following example, logical processor #0 and #2 share L1 and L2 caches. In this case, do not assign LP #0 and LP #2 to different VMs that need to do cache isolation. Assign LP #1 and LP #3 with similar consideration.

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

#. Similary bandwidth control is per-core (not per LP), so max delay values of per-LP CLOS is applied to the core. If HT is turned on, don’t place high priority threads on sibling LP running lower priority threads.

#. Based on scenario, build the ACRN hypervisor and copy the artifact ``acrn.efi`` to the
   ``/boot/EFI/acrn`` directory. If needed, update the devicemodel ``acrn-dm`` as well in ``/usr/bin`` directory. see :ref:`getting-started-building` for building instructions.
   
   .. code-block:: none

      $ make hypervisor BOARD=apl-up2 FIRMWARE=uefi
      ...

      # these operations are done on UP2 board
      $ mount /dev/mmcblk0p0 /boot
      $ scp <acrn.efi-at-your-compile-PC> /boot/EFI/acrn

#. Restart the platform

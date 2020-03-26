.. _hv_rdt:

RDT Allocation Feature Supported by Hypervisor
##############################################

The hypervisor uses RDT (Resource Director Technology) allocation features such as CAT(Cache Allocation Technology) and MBA(Memory Bandwidth Allocation) to control VMs which may be over-utilizing cache resources or memory bandwidth relative to their priority. By setting limits to critical resources, ACRN can optimize RTVM performance over regular VMs. In ACRN, the CAT and MBA are configured via the "VM-Configuration". The resources allocated for VMs are determined in the VM configuration(:ref:`rdt_vm_configuration`).

For futher details on Intel RDT, please refer to `Intel (R) 64 and IA-32 Architectures Software Developer's Manual, (Section 17.19 INTEL® RESOURCE DIRECTOR TECHNOLOGY ALLOCATION FEATURES) <https://software.intel.com/en-us/download/intel-64-and-ia-32-architectures-sdm-combined-volumes-3a-3b-3c-and-3d-system-programming-guide>`_


Objective of CAT
****************
The CAT feature in the hypervisor can isolate the cache for a VM from other VMs. It can also isolate the cache usage between VMX root mode and VMX non-root mode. Generally, certain cache resources will be allocated for the RT VMs in order to reduce the performance interference through the shared cache access from the neighbor VMs.

The figure below shows that with CAT, the cache ways can be isolated vs default where high priority VMs can be impacted by a noisy neighbor.

   .. figure:: images/cat-objective.png
      :align: center

CAT Support in ACRN
===================
On x86 platforms that support CAT, ACRN hypervisor automatically enables the support and by default shares the cache ways equally between all the VMs. This is done by setting max cache mask in MSR_IA32_type_MASK_n (where type: L2 or L3) MSR corresponding to each CLOS and setting IA32_PQR_ASSOC MSR with CLOS 0. The user can check the cache capabilities such as cache mask, max supported CLOS as described in :ref:`rdt_detection_capabilities` and program the IA32_type_MASK_n and IA32_PQR_ASSOC MSR with class-of-service (CLOS) ID, to select a cache mask to take effect. ACRN uses VMCS MSR loads on every VM Entry/VM Exit for non-root and root modes to enforce the settings.

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

   .. code-block:: none
      :emphasize-lines: 6

      struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] __aligned(PAGE_SIZE) = {
              {
                      .type = SOS_VM,
                      .name = SOS_VM_CONFIG_NAME,
                      .guest_flags = 0UL,
                      .clos = 0,
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
   ACRN takes the lowest common CLOS max value between the supported resources and sets the MAX_PLATFORM_CLOS_NUM. For example, if max CLOS supported by L3 is 16 and L2 is 8, ACRN programs MAX_PLATFORM_CLOS_NUM to 8. ACRN recommends to have consistent capabilities across all RDT resource by using common subset CLOS. This is done in order to minimize misconfiguration errors.


Objective of MBA
****************
The Memory Bandwidth Allocation (MBA) feature provides indirect and approximate control over memory bandwidth available per-core. It provides a method to control VMs which may be over-utilizing bandwidth relative to their priority and thus improving performance of high priority VMs. MBA introduces a programmable request rate controller (PRRC) between cores and high-speed interconnect. Throttling values can be programmed via MSRs to the PRRC to limit bandwidth availability.

The following figure shows memory bandwidth impact without MBA which cause bottleneck for high priority VMs vs with MBA support,

.. figure:: images/no_mba_objective.png
   :align: center
   :name: without-mba-support

   Without MBA Support

.. figure:: images/mba_objective.png
   :align: center
   :name: with-mba-support

   With MBA Support


MBA Support in ACRN
===================
On x86 platforms that support MBA, ACRN hypervisor automatically enables the support and by default sets no limits to the memory bandwidth access by VMs. This is done by setting 0 mba delay value in MSR_IA32_MBA_MASK_n MSR corresponding to each CLOS and setting IA32_PQR_ASSOC MSR with CLOS 0. The user can check the MBA capabilities such as mba delay values, max supported CLOS as described in :ref:`rdt_detection_capabilities` and program the IA32_MBA_MASK_n and IA32_PQR_ASSOC MSR with class-of-service (CLOS) ID, to select a delay to take effect for restricting memory bandwidth. ACRN uses VMCS MSR loads on every VM Entry/VM Exit for non-root and root modes to enforce the settings.

   .. code-block:: none
      :emphasize-lines: 3,7,11,15

      struct platform_clos_info platform_mba_clos_array[MAX_PLATFORM_CLOS_NUM] = {
              {
                      .mba_delay = 0,
                      .msr_index = MSR_IA32_MBA_MASK_BASE + 0,
              },
              {
                      .mba_delay = 0,
                      .msr_index = MSR_IA32_MBA_MASK_BASE + 1,
              },
              {
                      .mba_delay = 0,
                      .msr_index = MSR_IA32_MBA_MASK_BASE + 2,
              },
              {
                      .mba_delay = 0,
                      .msr_index = MSR_IA32_MBA_MASK_BASE + 3,
              },
      };

   .. code-block:: none
      :emphasize-lines: 6

      struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] __aligned(PAGE_SIZE) = {
              {
                      .type = SOS_VM,
                      .name = SOS_VM_CONFIG_NAME,
                      .guest_flags = 0UL,
                      .clos = 0,
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
   ACRN takes the lowest common CLOS max value between the supported resources and sets the MAX_PLATFORM_CLOS_NUM. For example, if max CLOS supported by L3 is 16 and MBA is 8, ACRN programs MAX_PLATFORM_CLOS_NUM to 8. ACRN recommends to have consistent capabilities across all RDT resource by using common subset CLOS. This is done in order to minimize misconfiguration errors.


CAT and MBA high-level design in ACRN
*************************************

Data structures
===============
The below figure shows the RDT data structure to store the enumerated resources.

   .. figure:: images/mba_data_structures.png
      :align: center

Enabling CAT, MBA software flow
===============================

The hypervisor enumerates RDT capabilities and sets up mask arrays; it also sets up CLOS for VMs and hypervisor itself per the "vm configuration"(:ref:`rdt_vm_configuration`).

* The RDT capabilities are enumerated on boot-strap processor (BSP), at the pCPU pre-initialize stage. The global data structure ``res_cap_info`` stores the capabilites of the supported resources.
* If CAT or/and MBA is supported, then setup masks array on all APs, at the pCPU post-initialize stage. The mask values are written to IA32_type_MASK_n. Refer :ref:`rdt_detection_capabilities` for details on identifying values to program the mask/delay MRSs as well as max CLOS.
* If CAT or/and is supported, the CLOS of a **VM** will be stored into its vCPU ``msr_store_area`` data structure guest part. It will be loaded to MSR IA32_PQR_ASSOC at each VM entry.
* If CAT or/and MBA is supported, the CLOS of **hypervisor** is stored for all VMs, in their vCPU ``msr_store_area`` data structure host part. It will be loaded to MSR IA32_PQR_ASSOC at each VM exit.

The figure below shows the high level overview of RDT resource flow in ACRN hypervisor.

   .. figure:: images/cat_mba_software_flow.png
      :align: center

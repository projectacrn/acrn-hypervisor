.. _hv_rdt:

RDT Allocation Feature Supported by Hypervisor
##############################################

The hypervisor uses RDT (Resource Director Technology) allocation features to optimize VM performance. There are 2 sub-features: CAT (Cache Allocation Technology) and MBA (Memory Bandwidth Allocation). CAT is for cache resources and MBA is for memory bandwidth resources. Code and Data Prioritization (CDP) is an extension of CAT. Only CAT is enabled due to the feature availability on an ACRN-supported platform. In ACRN, the CAT is configured via the "VM-Configuration". The resources allocated for VMs are determined in the VM configuration.

CAT Support in ACRN
*******************

Introduction to CAT Capabilities
================================

On a platform which supports CAT, each CPU can mask last-level-cache (LLC) with a cache mask, the masked cache ways cannot be evicted by this CPU. In terms of SDM, please see chapter 17, volume 3, CAT capabilities are enumerated via CPUID, and configured via MSR registers, these are:

* CPUID.10H contains CAT capabilities, such as cache mask type(L2/L3), mask bit
  length, number of masks.

* Cache masks are set in IA32_type_MASK_n, each one of these MSRs can
  hold a cache mask. They are shared by CPUs who share the same LLC; CPU sets
  the RMID field of its IA32_PQR_ASSOC MSR with class-of-service (CLOS) ID, to
  select a cache mask to take effect.

Objective of CAT
================

The CAT feature in the hypervisor can isolate the cache for a VM from other VMs. It can also isolate the cache usage between VMX root mode and VMX non-root mode. Generally, certain cache resources will be allocated for the RT VMs in order to reduce the performance interference through the shared cache access from the neighbour VMs.

CAT Workflow
=============

The hypervisor enumerates CAT capabilities and setup cache mask arrays; it also sets up CLOS for VMs and hypervisor itself per the "vm configuration".

* The CAT capabilities are enumerated on boot-strap processor (BSP), at the
  PCPU pre-initialize stage. The global data structure cat_cap_info holds the
  result.
* If CAT is supported, then setup cache masks array on all APs, at the PCPU
  post-initialize stage. The mask values are written to IA32_type_MASK_n. In
  fact, for CPUs which share LLC, they share the same IA32_type_MASK_n MSRs too,
  only need to do that on one CPU of them. The hypervisor does not detect
  hierarchy of LLCs.
* If CAT is supported. The CLOS of a VM will be stored into its vCPU
  msr_store_area data structure guest part. It will be loaded to
  MSR IA32_PQR_ASSOC at each VM entry.
* If CAT is supported, The CLOS of hypervisor is stored for all VMs, in their
  vCPU msr_store_area data structure host part. It will be loaded to MSR
  IA32_PQR_ASSOC at each VM exit.

.. _sgx_virt:

Enable SGX Virtualization
#########################

SGX refers to `Intel® Software Guard Extensions <https://software.intel.com/
en-us/sgx>`_ (Intel® SGX). This is a set of instructions that can be used by
applications to set aside protected areas for select code and data in order to
prevent direct attacks on executing code or data stored in memory. SGX allows
an application to instantiate a protected container, referred to as an
enclave, which is protected against external software access, including
privileged malware.


High Level ACRN SGX Virtualization Design
*****************************************

ACRN SGX virtualization support can be divided into three parts:

* SGX capability exposed to Guest
* EPC (Enclave Page Cache) management
* Enclave System function handling

The image below shows the high-level design of SGX virtualization in ACRN.

.. figure:: images/sgx-1.png
   :width: 500px
   :align: center

   SGX Virtualization in ACRN


Enable SGX support for Guest
****************************

Presumptions
============

No Enclave in a Hypervisor
--------------------------

ACRN does not support running an enclave in a hypervisor since the whole
hypervisor is currently running in VMX root mode, ring 0, and an enclave must
run in ring 3. ACRN SGX virtualization in provides the capability to (non-SOS)
VMs.

Enable SGX on Host
------------------

For SGX virtualization support in ACRN, you must manually enable the SGX
feature and configure the Processor Reserved Memory (PRM) in the platform
BIOS. (ACRN does not support the "Software Control" option to enable SGX at
run time.) If SGX is not enabled or the hardware platform does not support
SGX, ACRN SGX virtualization will not be enabled.

EPC Page Swapping in Guest
--------------------------

ACRN only partitions the physical EPC resources for VMs. The Guest OS kernel
handles EPC page swapping inside Guest.

Instructions
============

SGX support for a Guest OS is not enabled by default. Follow these steps to
enable SGX support in the BIOS and in ACRN:

#. Check the system BIOS on your target platform to see if Intel SGX is
   supported (CPUID.07H.EBX[2] should be 1).
#. Enable the SGX feature in the BIOS setup screens. Follow these instructions:

   a) Go to the Security page:

      .. figure:: images/sgx-2.jpg
         :width: 500px
         :align: center

   #) Enable SGX and configure the SGX Reserved Memory size as below:

      * Intel Software Guard Extension (SGX) -> Enabled
      * SGX Reserved Memory Size -> 128MB

      .. figure:: images/sgx-3.jpg
         :width: 500px
         :align: center

      .. note::

         Not all SGX Reserved Memory can be used as EPC. On KBL-NUC-i7,
         the SGX EPC size is 0x5d80000 (93.5MB) when the SGX Reserved Memory
         Size is set to 128MB.

#. Add the EPC config in the VM configuration.

   Apply the patch to enable SGX support in User VM in the SDC scenario:

   .. code-block:: bash

      $ cd <projectacrn base folder>
      $ curl https://github.com/binbinwu1/acrn-hypervisor/commit/0153b2b9b9920b61780163f19c6f5318562215ef.patch | git apply

#. Enable SGX in Guest:

   * **For a Linux Guest**, follow these `Linux SGX build instructions
     <https://github.com/intel/linux-sgx>`_
     to build and install the SGX driver and the SGX SDK and PSW packages.
   * **For a Windows Guest**, follow these `Windows SGX build instructions
     <https://software.intel.com/en-us/articles/getting-started-with-sgx-sdk-for-windows>`_
     for enabling applications with Intel SGX using Microsoft* Visual Studio*
     2015 on a 64-bit Microsoft Windows* OS.

SGX Capability Exposure
***********************
ACRN exposes SGX capability and EPC resource to a guest VM via CPUIDs and
Processor Model-Specific Registers (MSRs), as explained in the following
sections.

CPUID Virtualization
====================

CPUID Leaf 07H
--------------

* CPUID_07H.EAX[2] SGX: Supports Intel Software Guard Extensions if 1. If SGX
  is supported in Guest, this bit will be set.

* CPUID_07H.ECX[30] SGX_LC: Supports SGX Launch Configuration if 1. Currently,
  ACRN does not support the SGX Launch Configuration. This bit will not be
  set. Thus, the Launch Enclave must be signed by the Intel SGX Launch Enclave
  Key.

CPUID Leaf 12H
--------------

**Intel SGX Capability Enumeration**

* CPUID_12H.0.EAX[0] SGX1: If 1, indicates that Intel SGX supports the
  collection of SGX1 leaf functions. If is_sgx_supported and the section count
  is initialized for the VM, this bit will be set.
* CPUID_12H.0.EAX[1] SGX2: If 1, indicates that Intel SGX supports the
  collection of SGX2 leaf functions. If hardware supports it and SGX enabled
  for the VM, this bit will be set.
* Other fields of CPUID_12H.0.EAX aligns with the physical CPUID.

**Intel SGX Attributes Enumeration**

* CPUID_12H.1.EAX & CPUID_12H.1.EBX aligns with the physical CPUID.
* CPUID_12H.1.ECX & CPUID_12H.1.EDX reflects the allow-1 setting in the
  Extended feature (same structure as XCR0).

The hypervisor may change the allow-1 setting of XFRM in ATTRIBUTES for VM.
If some feature is disabled for the VM, the bit is also cleared, e.g. MPX.

**Intel SGX EPC Enumeration**

* CPUID_12H.2: The hypervisor presents only one EPC section to Guest. This
  vcpuid value will be constructed according to the EPC resource allocated to
  Guest.

MSR Virtualization
==================

IA32_FEATURE_CONTROL
--------------------

The hypervisor will opt-in to SGX for VM if SGX is enabled for VM.

* MSR_IA32_FEATURE_CONTROL_LOCK is set
* MSR_IA32_FEATURE_CONTROL_SGX_GE is set
* MSR_IA32_FEATURE_CONTROL_SGX_LC is not set

IA32_SGXLEPUBKEYHASH[0-3]
-------------------------

This is read-only since SGX LC is currently not supported.

SGXOWNEREPOCH[0-1]
------------------

* This is a 128-bit external entropy value for key derivation of an enclave.
* These MSRs are at the package level; they cannot be controlled by the VM.

EPC Virtualization
==================

* EPC resource is statically partitioned according to the configuration of the
  EPC size of VMs.
* During platform initialization, the physical EPC section information is
  collected via CPUID. SGX initialization function allocates EPC resource to
  VMs according to the EPC config in VM configurations.
* If enough EPC resource is allocated for the VM, assign the GPA of the EPC
  section.
* EPC resource is allocated to the Non-SOS VM; the EPC base GPA is specified
  by the EPC config in the VM configuration.
* The corresponding range of memory space should be marked as reserved in E820.
* During initialization, the mapping relationship of EPC HPA and GPA is saved
  for building the EPT table later when the VM is created.

Enclave System Function Handling
********************************

A new "Enable ENCLS exiting" control bit (bit 15) is defined in the secondary
processor-based VM execution control.

* 1-Setting of "Enable ENCLS exiting" enables ENCLS-exiting bitmap control,
  which is a new 64-bit ENCLS-exiting bitmap control field added to VMX VMCS (
  0202EH) to control VMEXIT on ENCLS leaf functions.
* ACRN does not emulate ENCLS leaf functions and will not enable ENCLS exiting.

ENCLS[ECREATE]
==============

* The enclave execution environment is heavily influenced by the value of
  ATTRIBUTES in the enclave's SECS.
* When ECREATE is executed, the processor will check and verify that the
  enclave requirements are supported on the platform. If not, ECREATE will
  generate a #GP.
* The hypervisor can present the same extended features to Guest as the
  hardware. However, if the hypervisor hides some extended features that the
  hardware supports from the VM/guest, then if the hypervisor does not trap
  ENCLS[ECREATE], ECREATE may succeed even if the ATTRIBUTES the enclave
  requested is not supported in the VM.
* Fortunately, ENCLU[EENTER] will fault if SECS.ATTRIBUTES.XFRM is not a
  subset of XCR0 when CR4.OSXSAVE = 1.
* XCR0 is controlled by the hypervisor in ACRN; if the hypervisor hides some
  extended feature from the VM/guest, then ENCLU[EENTER] will fault if the
  enclave requests a feature that the VM does not support if the hypervisor
  does not trap/emulate ENCLS[ECREATE].
* Above all, the security feature is not compromised if the hypervisor does
  not trap ENCLS[ECREATE] to check the attributes of the enclave.

Other VMExit Control
********************

RDRAND exiting
==============

* ACRN allows Guest to use RDRAND/RDSEED instruction but does not set "RDRAND
  exiting" to 1.

PAUSE exiting
=============

* ACRN does not set "PAUSE exiting" to 1.

Future Development
******************
Following are some currently unplanned areas of interest for future
ACRN development around SGX virtualization.

Launch Configuration support
============================

When the following two conditions are both satisfied:

* The hardware platform supports the SGX Launch Configuration.

* The platform BIOS must enable the feature in Unlocked mode, so that the
  ring0 software can configure the Model Specific Register (MSR)
  IA32_SGXLEPUBKEYHASH[0-3] values.

the following statements apply:

* If CPU sharing is supported, ACRN can emulate MSR IA32_SGXLEPUBKEYHASH[0-3]
  for VM. ACRN updates MSR IA32_SGXLEPUBKEYHASH[0-3] when the VM context
  switch happens.
* If CPU sharing is not supported, ACRN can support SGX LC by passthrough MSR
  IA32_SGXLEPUBKEYHASH[0-3] to Guest.

ACPI Virtualization
===================

* The Intel SGX EPC ACPI device is provided in the ACPI Differentiated System
  Descriptor Table (DSDT), which contains the details of the Intel SGX
  existence on the platform as well as memory size and location.
* Although the EPC can be discovered by the CPUID, several versions of Windows
  do rely on the ACPI tables to enumerate the address and size of the EPC.

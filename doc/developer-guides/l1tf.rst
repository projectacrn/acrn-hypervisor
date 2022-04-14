.. _l1tf:

L1 Terminal Fault Mitigation
############################

Overview
********

Refer to `Intel Analysis of L1TF`_ and `Linux L1TF document`_ for details.

.. _Intel Analysis of L1TF:
   https://www.intel.com/content/www/us/en/developer/articles/technical/software-security-guidance/advisory-guidance/l1-terminal-fault.html

.. _Linux L1TF document:
   https://www.kernel.org/doc/html/latest/admin-guide/hw-vuln/l1tf.html

L1 Terminal Fault is a speculative side channel that allows unprivileged
speculative access to data that is available in the Level 1 Data Cache
when the page table entry controlling the virtual address, used
for the access, has the present bit cleared or reserved bits set.

When the processor accesses a linear address, it first looks for a
translation to a physical address in the translation lookaside buffer (TLB).
For an unmapped address this will not provide a physical address, so the
processor performs a table walk of a hierarchical paging structure in
memory that provides translations from linear to physical addresses. A page
fault is signaled if this table walk fails.

During the process of a terminal fault, the processor speculatively computes
a physical address from the paging structure entry and the address of the
fault. This physical address is composed of the address of the page frame
and low order bits from the linear address. If data with this physical
address is present in the L1D, that data may be loaded and forwarded to
dependent instructions. These dependent instructions may create a side
channel.

Because the resulting probed physical address is not a true translation of
the virtual address, the resulting address is not constrained by various
memory range checks or nested translations. Specifically:

* Intel |reg| SGX protected memory checks are not applied.
* Extended Page Table (EPT) guest physical to host physical address
  translation is not applied.
* SMM protected memory checks are not applied.

The following CVE entries are related to the L1TF:

=============  =================  ==============================
CVE-2018-3615  L1 Terminal Fault  Intel SGX related aspects
CVE-2018-3620  L1 Terminal Fault  OS, SMM related aspects
CVE-2018-3646  L1 Terminal Fault  Virtualization related aspects
=============  =================  ==============================

L1TF Problem in ACRN
********************

There are mainly three attack scenarios considered in ACRN:

- Guest -> hypervisor attack
- Guest -> guest attack
- Normal_world -> secure_world attack (Android specific)

Malicious user space is not a concern to ACRN hypervisor, because
every guest runs in VMX non-root. It is responsibility of guest kernel
to protect itself from malicious user space attack.

Intel SGX/SMM related attacks are mitigated by using latest microcode.
There is no additional action in ACRN hypervisor.

Guest -> Hypervisor Attack
==========================

ACRN always enables EPT for all guests (Service VM and User VM); thus a
malicious guest can directly control guest PTEs to construct an L1TF-based
attack to the hypervisor. Alternatively, if ACRN EPT is not sanitized with
some PTEs (with present bit cleared, or reserved bit set) pointing to valid
host PFNs, a malicious guest can use those EPT PTEs to construct an attack.

A special aspect of L1TF in the context of virtualization is symmetric
multithreading (SMT), e.g. Intel |reg| Hyper-threading Technology.
Logical processors on the affected physical cores share the L1 Data Cache
(L1D). This fact could produce more variants of L1TF-based attack; e.g.,
a malicious guest running on one logical processor can attack the data
brought into L1D by the context that runs on the sibling thread of the same
physical core. This context can be any code in the hypervisor.

Guest -> Guest Attack
=====================

The possibility of guest -> guest attack varies by specific configuration,
e.g. whether CPU partitioning is used, whether Hyper-threading is on, etc.

If CPU partitioning is enabled (the default policy in ACRN), there is a 1:1
mapping between vCPUs and pCPUs; that is, there is no pCPU sharing. There
may be an attack possibility when Hyper-threading is on, where logical
processors of the same physical core may be allocated to two different
guests. Then one guest may be able to attack the other guest on a sibling
thread due to shared L1D.

If CPU sharing is enabled (not supported now), two VMs may share the same
pCPU; thus the next VM may steal information in L1D that comes from activity
of the previous VM on the same pCPU.

Normal_world -> Secure_world Attack
===================================

ACRN supports Android guest, which requires two running worlds (normal world
and secure world). Two worlds run on the same CPU, and world switch is
conducted on demand. It could be possible for normal world to construct an
L1TF-based stack to secure world, breaking the security model as expected by
the Android guest.

Affected Processors
===================

L1TF affects a range of Intel processors, but Intel Atom |reg| processors
are immune to it.

Processors that have the RDCL_NO bit set to one (1) in the
IA32_ARCH_CAPABILITIES MSR are not susceptible to the L1TF
speculative execution side channel.

For more details, refer to `Intel Analysis of L1TF`_.

L1TF Mitigation in ACRN
***********************

Use the latest microcode, which mitigates SMM and Intel SGX cases while also
providing necessary capability for VMM to use for further mitigation.

ACRN will check the platform capability based on `CPUID enumeration
and architectural MSR`_. For an L1TF affected platform (CPUID.07H.EDX.29
with MSR_IA32_ARCH_CAPABILITIES), L1D_FLUSH capability (CPUID.07H.EDX.28)
must be supported.

.. _CPUID enumeration and architectural MSR:
   https://www.intel.com/content/www/us/en/developer/articles/technical/software-security-guidance/technical-documentation/cpuid-enumeration-and-architectural-msrs.html

Not all the mitigations below will be implemented in ACRN, and not all of
them apply to a specific ACRN deployment. Check the `Mitigation Status`_ and
`Mitigation Recommendations`_ sections for guidance.

L1D Flush on VMENTRY
====================

ACRN may optionally flush L1D at VMENTRY, which ensures that no sensitive
information from the hypervisor or previous VM is revealed to the current VM
(in case of CPU sharing).

Flushing the L1D evicts not only the data that should not be accessed by a
potentially malicious guest, it also flushes the guest data. Flushing the
L1D has a performance impact as the processor has to bring the flushed guest
data back into the L1D, and the actual overhead is proportional to the
frequency of vmentry.

Due to such performance reasons, ACRN provides a config option
(L1D_FLUSH_VMENTRY) to enable/disable L1D flush during VMENTRY. By default,
this option is disabled.

EPT Sanitization
================

EPT is sanitized to avoid pointing to valid host memory in PTEs that have
the present bit cleared or reserved bits set.

For non-present PTEs, ACRN sets PFN bits to ZERO, which means
that page ZERO might fall into risk if it contains security information.
ACRN reserves page ZERO (0~4K) from page allocator; thus page ZERO won't
be used by anybody for a valid purpose. This sanitization logic is always
enabled on all platforms.

ACRN hypervisor doesn't set reserved bits in any EPT entry.

Put Secret Data Into Uncached Memory
====================================

It is hard to decide which data in ACRN hypervisor is secret or valuable
data. The amount of valuable data from ACRN contexts cannot be declared as
non-interesting for an attacker without deep inspection of the code.

But obviously, the most import secret data in ACRN is the physical platform
seed generated from CSME and virtual seeds derived from that platform seed.
They are critical secrets to serve for a guest keystore or other security
usage, e.g. disk encryption, secure storage.

If the critical secret data in ACRN is identified, then such data can be put
into un-cached memory. As the content will never go to L1D, it is immune to
L1TF attack.

For example, after getting the physical seed from CSME, before any guest
starts, ACRN can pre-derive all the virtual seeds for all the guests and
then put these virtual seeds into uncached memory, and at the same time
flush and erase the physical seed.

If all security data are identified and put in uncached memory in a specific
deployment, it is not necessary to prevent guest -> hypervisor attack,
because there is nothing useful to be attacked.

However, if such 100% identification is not possible, the user should
consider other mitigation options to protect the hypervisor.

L1D Flush on World Switch
=========================

For L1D-affected platforms, ACRN writes to aforementioned MSR to flush L1D
when switching from secure world to normal world. Doing so guarantees that
no sensitive information from secure world leaked into L1D. The performance
impact is expected to be small since world switch frequency is not expected
to be high.

It's not necessary to flush L1D in the other direction, because normal world
is a less privileged entity than secure world.

This mitigation is always enabled.

Core-Based Scheduling
=====================

If Hyper-threading is enabled, it's important to avoid running a sensitive
context (if it contains security data that a given VM has no permission to
access) on the same physical core that runs that VM. It requires a scheduler
enhancement to enable a core-based scheduling policy, so all threads on the
same core are always scheduled to the same VM. Also there are some further
actions required to protect the hypervisor and secure the world from sibling
attacks in the core-based scheduler.

.. note:: There is no current plan to implement this scheduling policy. The
   ACRN community will evaluate the need for this based on usage
   requirements and hardware platform status.

Mitigation Recommendations
**************************

There is no mitigation required on Apollo Lake based platforms.

The majority use case for ACRN is in a pre-configured environment, where the
whole software stack (from ACRN hypervisor to guest kernel to Service VM
root) is tightly controlled by the solution provider and not enabled for
runtime change after sale (that is, the guest kernel is trusted). In that
case, the solution provider will make sure that the guest kernel is
up-to-date including necessary page table sanitization; thus there is no
attack interface exposed within the guest. Then a minimal mitigation
configuration is sufficient with negligible performance impact, as
explained below:

1) Use latest microcode
2) Guest kernel is up-to-date with page table sanitization
3) EPT sanitization (always enabled)
4) Flush L1D at world switch (Android specific, always enabled)

In case someone wants to deploy ACRN into an open environment where the
guest kernel is considered untrusted, there are additional mitigation
options required according to the specific usage requirements:

5) Put hypervisor security data in UC memory if possible
6) Enable L1D_FLUSH_VMENTRY option, if

   - Doing 5) is not feasible, or
   - CPU sharing is enabled (in the future)

If Hyper-threading is enabled, there is no available mitigation option
before core scheduling is planned. The user should understand the security
implication and only turn on Hyper-threading when the potential risk is
acceptable for their usage.

Mitigation Status
*****************

===========================  =============
Mitigation                   status
===========================  =============
EPT sanitization             supported
L1D flush on VMENTRY         supported
L1D flush on world switch    supported
Uncached security data       n/a
Core scheduling              n/a
===========================  =============

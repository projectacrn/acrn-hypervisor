.. _l1tf:

L1 Terminal Fault Mitigation
############################

Overview
********

Refer to `Intel Analysis of L1TF`_ and `Linux L1TF document`_ for details.

.. _Intel Analysis of L1TF:
   https://software.intel.com/security-software-guidance/insights/deep-dive-intel-analysis-l1-terminal-fault

.. _Linux L1TF document:
   https://github.com/torvalds/linux/blob/master/Documentation/admin-guide/l1tf.rst

L1 Terminal Fault is a speculative side channel which allows unprivileged
speculative access to data which is available in the Level 1 Data Cache
when the page table entry controlling the virtual address, which is used
for the access, has the Present bit cleared or reserved bits set.

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

* Intel® SGX protected memory checks are not applied.
* Extended Page Table (EPT) guest physical to host physical address
  translation is not applied.
* SMM protected memory checks are not applied.

The following CVE entries are related to the L1TF:

=============  =================  ==============================
CVE-2018-3615  L1 Terminal Fault  Intel® SGX related aspects
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
every guest runs in VMX non-root. It is reponsibility of guest kernel
to protect itself from malicious user space attack.

Intel® SGX/SMM related attacks are mitigated by using latest microcode.
There is no additional action in ACRN hypervisor.

Guest -> hypervisor Attack
==========================

ACRN always enables EPT for all guests (SOS and UOS), thus a malicious
guest can directly control guest PTEs to construct L1TF-based attack
to hypervisor. Alternatively if ACRN EPT is not sanitized with some
PTEs (with present bit cleared, or reserved bit set) pointing to valid
host PFNs, a malicious guest may use those EPT PTEs to construct an attack.

A special aspect of L1TF in the context of virtualization is symmetric
multi threading (SMT), e.g. Intel® Hyper-Threading Technology.
Logical processors on the affected physical cores share the L1 Data Cache
(L1D). This fact could make more variants of L1TF-based attack, e.g.
a malicious guest running on one logical processor can attack the data which
is brought into L1D by the context which runs on the sibling thread of
the same physical core. This context can be any code in hypervisor.

Guest -> guest Attack
=====================

The possibility of guest -> guest attack varies on specific configuration,
e.g. whether CPU partitioning is used, whether Hyper-Threading is on, etc.

If CPU partitioning is enabled (default policy in ACRN), there is
1:1 mapping between vCPUs and pCPUs i.e. no sharing of pCPU. There
may be an attack possibility when Hyper-Threading is on, where
logical processors of same physical core may be allocated to two 
different guests. Then one guest may be able to attack the other guest
on sibling thread due to shared L1D.

If CPU sharing is enabled (not supported now), two VMs may share
same pCPU thus next VM may steal information in L1D which comes
from activity of previous VM on the same pCPU.

Normal_world -> Secure_world Attack
===================================

ACRN supports Android guest, which requires two running worlds
(normal world and secure world). Two worlds run on the same CPU,
and world switch is conducted on demand. It could be possible for
normal world to construct an L1TF-based stack to secure world,
breaking the security model as expected by Android guest.

Affected Processors
===================

L1TF affects a range of Intel processors, but Intel ATOM® processors
(including Apollo Lake) are immune to it. Currently ACRN hypervisor
supports only Apollo Lake. Support for other core-based platforms is
planned, so we still need a mitigation plan in ACRN.

Processors that have the RDCL_NO bit set to one (1) in the
IA32_ARCH_CAPABILITIES MSR are not susceptible to the L1TF
speculative execution side channel.

Please refer to `Intel Analysis of L1TF`_ for more details.

L1TF Mitigation in ACRN
***********************

Use the latest microcode, which mitigates SMM and Intel® SGX cases
while also providing necessary capability for VMM to use for further
mitigation.

ACRN will check the platform capability based on `CPUID enumeration
and architectural MSR`_. For L1TF affected platform (CPUID.07H.EDX.29
with MSR_IA32_ARCH_CAPABILITIES), L1D_FLUSH capability(CPUID.07H.EDX.28)
must be supported.

.. _CPUID enumeration and architectural MSR:
   https://software.intel.com/security-software-guidance/insights/deep-dive-cpuid-enumeration-and-architectural-msrs

Not all of mitigations below will be implemented in ACRN, and
not all of them apply to a specific ACRN deployment. Check the
'Mitigation Status'_ and 'Mitigation Recommendations'_ sections
for guidance.

L1D flush on VMENTRY
====================

ACRN may optionally flush L1D at VMENTRY, which ensures no
sensitive information from hypervisor or previous VM revealed
to current VM (in case of CPU sharing).

Flushing the L1D evicts not only the data which should not be
accessed by a potentially malicious guest, it also flushes the
guest data. Flushing the L1D has a performance impact as the 
processor has to bring the flushed guest data back into the L1D,
and actual overhead is proportional to the frequency of vmentry.

Due to such performance reason, ACRN provides a config option
(L1D_FLUSH_VMENTRY) to enable/disable L1D flush during
VMENTRY. By default this option is disabled.

EPT Sanitization
================

EPT is sanitized to avoid pointing to valid host memory in PTEs
which has present bit cleared or reserved bits set.

For non-present PTEs, ACRN currently set pfn bits to ZERO, which
means page ZERO might fall into risk if containing security info.
ACRN reserves page ZERO (0~4K) from page allocator thus page ZERO
won't be used by anybody for valid usage. This sanitization logic
is always enabled on all platforms.

ACRN hypervisor doesn't set reserved bits in any EPT entry.

Put Secret Data into Uncached Memory
====================================

It is hard to decide which data in ACRN hypervisor is secret or valuable
data. The amount of valuable data from ACRN contexts cannot be declared as
non-interesting for an attacker without deep inspection of the code.

But obviously, the most import secret data in ACRN is the physical platform
seed generated from CSME and virtual seeds which are derived from that
platform seed. They are critical secrets to serve for guest keystore or
other security usage, e.g. disk encryption, secure storage.

If the critical secret data in ACRN is identified, then such
data can be put into un-cached memory. As the content will 
never go to L1D, it is immune to L1TF attack

For example, after getting the physical seed from CSME, before any guest
starts, ACRN can pre-derive all the virtual seeds for all the
guests and then put these virtual seeds into uncached memory,
at the same time flush & erase physical seed.

If all security data are identified and put in uncached
meomry in a specific deployment, then it is not necessary to
prevent guest -> hypervisor attack, since there is nothing
useful to be attacked.

However if such 100% identification is not possible, user should
consider other mitigation options to protect hypervisor.

L1D flush on World Switch
=========================

For L1D-affected platforms, ACRN writes to aforementioned MSR
to flush L1D when switching from secure world to normal world.
Doing so guarantees no sensitive information from secure world
leaked in L1D. Performance impact is expected to small since world
switch frequency is not expected high.

It's not necessary to flush L1D in the other direction, since
normal world is less privileged entity to secure world.

This mitigation is always enabled.

Core-based scheduling
=====================

If Hyper-Threading is enabled, it's important to avoid running
sensitive context (if containing security data which a given VM
has no premission to access) on the same physical core that runs
said VM. It requires scheduler enhancement to enable core-based
scheduling policy, so all threads on the same core are always
scheduled to the same VM. Also there are some further actions
required to protect hypervisor and secure world from sibling
attacks in core-based scheduler.

.. note:: There is no current plan to implement this scheduling
  policy. The ACRN community will evaluate the need for this based
  on usage requirements and hardware platform status.

Mitigation Recommendations
**************************

There is no mitigation required on Apollo Lake based platforms.

The majority use case for ACRN is in pre-configured environment,
where the whole software stack (from ACRN hypervisor to guest 
kernel to SOS root) is tightly controlled by solution provider
and not allowed for run-time change after sale (guest kernel is
trusted). In that case solution provider will make sure that guest
kernel is up-to-date including necessary page table sanitization,
thus there is no attack interface exposed within guest. Then a
minimal mitigation configuration is sufficient with negligible
performance impact, as explained below:

1) Use latest microcode
2) Guest kernel is up-to-date with page table sanitization
3) EPT sanitization (always enabled)
4) Flush L1D at world switch (Android specific, always enabled)

In case that someone wants to deploy ACRN into an open environment
where guest kernel is considered untrusted, there are more
mitigation options required according to the specific usage
requirements:

5) Put hypervisor security data in UC memory if possible
6) Enable L1D_FLUSH_VMENTRY option, if

   - Doing 5) is not feasible, or
   - CPU sharing is enabled (in the future)

If Hyper-Threading is enabled, there is no available mitigation
option before core scheduling is planned. User should understand
the security implication and only turn on Hyper-Threading
when the potential risk is acceptable to their usage.

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

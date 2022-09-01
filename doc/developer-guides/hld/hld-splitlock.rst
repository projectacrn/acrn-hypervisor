.. _hld_splitlock:

Handling Split-Locked Access in ACRN
####################################

A split lock is any atomic operation whose operand crosses two cache
lines. Because the operation must be atomic, the system locks the bus
while the CPU accesses the two cache lines.  Blocking bus access from
other CPUs plus the bus locking protocol overhead degrades overall
system performance.

This document explains Split-locked Access, how to detect it, and how
ACRN handles it.

Split-Locked Access Introduction
********************************
Intel-64 and IA32 multiple-processor systems support locked atomic
operations on locations in system memory. For example, The LOCK instruction
prefix can be prepended to the following instructions: ADD, ADC, AND, BTC, BTR, BTS,
CMPXCHG, CMPXCH8B, CMPXCHG16B, DEC, INC, NEG, NOT, OR, SBB, SUB, XOR, XADD,
and XCHG, when these instructions use memory destination operand forms.
Reading or writing a byte in system memory is always guaranteed to be
atomic, otherwise, these locked atomic operations can impact system in two
ways:

- **The destination operand is located in the same cache line.**

   Cache coherency protocols ensure that atomic operations can be
   carried out on cached data structures with cache lock.

- **The destination operand is located in two cache lines.**

   This atomic operation is called a Split-locked Access. For this situation,
   the LOCK# bus signal is asserted to lock the system bus, to ensure
   the operation is atomic. See `Intel 64 and IA-32 Architectures Software Developer's Manual (SDM), Volume 3, (Section 8.1.2 Bus Locking) <https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html>`_.

Split-locked Access can cause unexpected long latency to ordinary memory
operations by other CPUs while the bus is locked. This degraded system
performance can be hard to investigate.

Split-Locked Access Detection
*****************************
The `Intel Tremont Microarchitecture
<https://newsroom.intel.com/news/intel-introduces-tremont-microarchitecture>`_
introduced a new CPU capability for detecting Split-locked Access. When
this feature is enabled, an alignment check exception (#AC) with error
code 0 is raised for instructions causing a Split-locked Access. Because
#AC is a fault, the instruction is not executed, giving the #AC handler
an opportunity to decide how to handle this instruction:

- It can allow the instruction to run with LOCK# bus signal potentially
  impacting performance of other CPUs.
- It can disable LOCK# assertion for split locked access, but
  improperly makes the instruction non-atomic.
- It can terminate the software at this instruction.

Feature Enumeration and Control
*******************************
#AC for Split-locked Access feature is enumerated and controlled via CPUID and
MSR registers.

- CPUID.(EAX=0x7, ECX=0):EDX[30], the 30th bit of output value in EDX indicates
  if the platform has IA32_CORE_CAPABILITIES MSR.

- The 5th bit of IA32_CORE_CAPABILITIES MSR(0xcf), enumerates whether the CPU
  supports #AC for Split-locked Access (and has TEST_CTRL MSR).

- The 29th bit of TEST_CTL MSR(0x33) controls enabling and disabling #AC for Split-locked
  Access.

ACRN Handling Split-Locked Access
*********************************
Split-locked Access is not expected in the ACRN hypervisor itself, and
should never happen. However, such access could happen inside a VM. ACRN
support for handling split-locked access follows these design principles:

- Always enable #AC on Split-locked Access for the physical processors.

- Present a virtual split lock capability to guest (VMs), and directly
  deliver the alignment check exception (#AC) to the guest. (This
  virtual split-lock capability helps the guest isolate violations from
  user land).

- Guest write of MSR_TEST_CTL is ignored, and guest read gets the written value.

- Any Split-locked Access in the ACRN hypervisor is a software bug we must fix.

- If split-locked Access happens in a guest kernel, the guest may not be able to
  fix the issue gracefully. (The guest may behave differently than the
  native OS). The real-time (RT) guest must avoid a Split-locked Access
  and consider it a software bug.

Enable Split-Locked Access Handling Early
==========================================
This feature is enumerated at the Physical CPU (pCPU) pre-initialization
stage, where ACRN detects CPU capabilities. If the pCPU supports this
feature:

- Enable it at each pCPU post-initialization stage.

- ACRN hypervisor presents a virtual emulated TEST_CTRL MSR to each
  Virtual CPU (vCPU).
  Setting or clearing TEST_CTRL[bit 29] in a vCPU, has no effect.

If pCPU does not have this capability, a vCPU does not have the virtual
TEST_CTRL either.

Expected Behavior in ACRN
=========================
The ACRN hypervisor should never trigger Split-locked Access and it is
not allowed to run with Split-locked Access. If ACRN does trigger a
split-locked access, ACRN reports #AC at the instruction and stops
running. The offending HV instruction is considered a bug that must be
fixed.

Expected Behavior in VM
=======================
If a VM process has a Split-locked Access in user space, it will be
terminated by SIGBUS. When debugging inside a VM, you may find it
triggers an #AC even if alignment checking is disabled.

If a VM kernel has a Split-locked Access, it will hang or oops on an
#AC. A VM kernel may try to disable #AC for Split-locked Access and
continue, but it will fail. The ACRN hypervisor helps identify the
problem by reporting a warning message that the VM tried writing to
TEST_CTRL MSR.


Disable Split-Locked Access Detection
=====================================
If the CPU supports Split-locked Access detection, the ACRN hypervisor
uses it to prevent any VM running with potential system performance
impacting split-locked instructions. This detection can be disabled
(by deselecting the :term:`Enable split lock detection` option in
the ACRN Configurator tool) for customers not
caring about system performance.

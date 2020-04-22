.. _hld_splitlock:

Handling Split-locked Access in Acrn
####################################

This document explains what is Split-locked Access, how to detect it and what
Acrn do for handling it.

Split-locked Access Introduction
********************************
The Intel-64 and IA32 multiple-processor systems support locked atomic
operations on locations in system memory. For example, The LOCK instruction
prefix be prepended to the following instructions: ADD, ADC, AND, BTC, BTR, BTS,
CMPXCHG, CMPXCH8B, CMPXCHG16B, DEC, INC, NEG, NOT, OR, SBB, SUB, XOR, XADD,
XCHG. and these instructions must with memory destination operand forms.
Reading or writing a byte in system memory are always guaranteed to be handled
atomically, otherwise, these locked atomic operations can impact system in two
ways:

- The destination operand located in same cache line. Cache coherency protocols
  that ensure that atomic operations can be carried out on cached data
  structures with cache lock.

- The destination operand located in two cache line. This atomic operation is
  called a Split-locked Access. For this situation, LOCK# bus signal is asserted
  to lock the system bus, to ensure they are atomic. See `Intel 64 and IA-32 Architectures Software Developer's Manual(SDM), Volume 3, (Section 8.1.2 Bus Locking) <https://software.intel.com/en-us/download/intel-64-and-ia-32-architectures-sdm-combined-volumes-3a-3b-3c-and-3d-system-programming-guide>`_.

Split-locked Access may lead to long latency, to ordinary memory operations on
other CPUs, which is hard to investigate.

Split-locked Access Detection
*****************************
From the Microarchitecture Tremont, a new CPU capability is introduced for
detection of Split-locked Access. When this feature is enabled, an alignment
check exception(#AC) with error code 0 is raised for instructions which are
doing Split-locked Access. #AC is a fault, the instruction is not executed,
there is a chance to decide what to do with this instruction in the #AC handler:

- Allow the instruction to run with LOCK# bus signal, may have impact to other
  CPUs.
- Disable LOCK# assertion for split locked access. That is not right because
  instruction becomes non-atomic, and Intel plans to remove this CPU feature
  from select products starting from a specific year. See `SDM, Volume 1, (Section 2.4 PROPOSED REMOVAL FROM UPCOMING PRODUCTS) <https://software.intel.com/en-us/download/intel-64-and-ia-32-architectures-software-developers-manual-volume-1-basic-architecture>`_.
- Terminate the software at this instruction.

Feature Enumeration and Control
*******************************
#AC for Split-locked Access feature is enumarated and controled via CPUID and
MSR registers.

- CPUID.(EAX=0x7, ECX=0):EDX[30], the 30th bit of output value in EDX indicates
  if the platform has IA32_CORE_CAPABILITIES MSR.

- The 5th bit of IA32_CORE_CAPABILITIES MSR(0xcf), enumerates whether the CPU
  support #AC for Split-locked Access(And has TEST_CTRL MSR).

- The 29th bit of TEST_CTL MSR(0x33) control enable/disable #AC for Split-locked
  Access.

Acrn Handling Split-locked Access
*********************************
Split-locked Access is not expected in Acrn hypervisor, should never happen. But
that is not guaranteed inside VMs. Acrn support use this CPU capability, that
are design principles:

- Always enable #AC on Split-locked Access in physical side.

- Present virtual split lock capability to guest, and directly deliver #AC to
  guest (virtual split-lock capability helps the guest to isolate violations
  from user land).

- Guest write of MSR_TEST_CTL is ignored, guest read gets the written value.

- If Split-locked Access happens in ACRN, this is a software bug we must fix it.

- If split-locked Access happens in guest kernel, the guest may not be able to
  fix the issue gracefully (the guest may behavior differently with that of
  native OS). The RT guest should avoid the Split-locked Access, otherwise it is
  a software bug.

Fore Enable This Feature
========================
This feature is enumerated at PCPU pre-initialization stage, where Acrn detects
 CPU capabilities. If the PCPU supports this feature:

- Enabled at each PCPU post-initialization stage.

- Acrn hypervisor presents virtual emulated TEST_CTRL MSR to each VCPU.
  Setting/clearing TEST_CTRL[bit 29] from VCPU, has no any effect.

If PCPU does not have this capability, VCPU does not have the virtual TEST_CTRL
neither.

Expected Behavior in Acrn
=========================
Acrn HV should never trigger Split-locked Access, it is not allowed to run with
Split-locked Access. If it does trigger split-lock, Acrn report #AC at the
instruction and stop running. This HV instruction is considered as a bug, must
fix it.

Expected Behavior in VM
=======================
If VM process have a Split-locked Access at user space, it will be terminated by
SIGBUS. When debug inside VM for a reason, you may find it triggers an #AC, no
matter alignment checking is enabled or not.
If VM kernel have a Split-locked Access, it would hang or oops for #AC. VM
kernel may try disable #AC for Split-locked Access and continue, but it will not
make it. For that case, you may find HV warning message that says VM tries
writing to TEST_CTRL MSR, and help to identify the problem.


Disable Split-locked Access Detection
=====================================
When the CPU support Split-locked Access detection, Acrn HV will use it to
prevent any VM to run a burden software(which contains split-locked instructions). Sometimes customer may really want to run the software and does not care
about performance of entire system, he can configure this feature out of Acrn
code.
The next work in the plan is to use Acrn Configuration Tools to disable split lock detection, that will be done later.

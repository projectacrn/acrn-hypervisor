.. _rt_perf_tips_rtvm:

ACRN Real-Time VM Performance Tips
##################################

Background
**********

The ACRN real-time VM (RTVM) is a special type of ACRN post-launched VM.
This document shows how you can configure RTVMs to potentially achieve
near bare-metal performance by configuring certain key technologies and
eliminating use of a VM-exit within RT tasks, thereby avoiding this
common virtualization overhead issue.

Neighbor VMs such as Service VMs, Human-Machine-Interface (HMI) VMs, or
other real-time VMs, may negatively affect the execution of real-time
tasks on an RTVM. This document also shows technologies used to isolate
potential runtime noise from neighbor VMs.

Here are some key technologies that can significantly improve
RTVM performance:

- LAPIC passthrough with core partitioning.
- PCIe Device Passthrough: Only MSI interrupt-capable PCI devices are
  supported for the RTVM.
- Enable CAT (Cache Allocation Technology)-based cache isolation: RTVM uses
  a dedicated CLOS (Class of Service). While others may share CLOS, the GPU
  uses a CLOS that will not overlap with the RTVM CLOS.
- PMD virtio: Both virtio BE and FE work in polling mode so
  interrupts and notification between the Service VM and RTVM are not needed.
  All RTVM guest memory is hidden from the Service VM except for the virtio
  queue memory.

This document summarizes tips from issues encountered and
resolved during real-time development and performance tuning.

Mandatory options for an RTVM
*****************************

An RTVM is a post-launched VM with LAPIC passthrough. Pay attention to
these options when you launch an ACRN RTVM:

Tip: Apply the acrn-dm option ``--lapic_pt``
   The LAPIC passthrough feature of ACRN is configured via the
   ``--lapic_pt`` option, but the feature is actually enabled when LAPIC is
   switched to X2APIC mode. Both conditions should be met to enable an
   RTVM. The ``--rtvm`` option will be automatically attached once
   ``--lapic_pt`` is applied.

Tip: Use virtio polling mode
   Polling mode prevents the frontend of the VM-exit from sending a
   notification to the backend.  We recommend that you passthrough a
   physical peripheral device (such as block or an ethernet device), to an
   RTVM. If no physical device is available, ACRN supports virtio devices
   and enables polling mode to avoid a VM-exit at the frontend. Enable
   virtio polling mode via the option ``--virtio_poll [polling interval]``.

Avoid VM-exit latency
*********************

VM-exit has a significant negative impact on virtualization performance.
A single VM-exit causes a several micro-second or longer latency,
depending on what's done in VMX-root mode. VM-exit is classified into two
types: triggered by external CPU events or triggered by operations initiated
by the vCPU.

ACRN eliminates almost all VM-exits triggered by external events by
using LAPIC passthrough. A few exceptions exist:

- SMI - This brings the processor into the SMM, causing a much longer
  performance impact. The SMI should be handled in the BIOS.

- NMI - ACRN uses NMI for system-level notification.

You should avoid VM-exits triggered by operations initiated by the
vCPU. Refer to the `Intel Software Developer Manuals (SDM)
<https://software.intel.com/en-us/articles/intel-sdm>`_ "Instructions
Cause VM-exits Unconditionally" (SDM V3, 25.1.2) and "Instructions That
Cause VM-exits Conditionally" (SDM V3, 25.1.3).

Tip: Do not use CPUID in a real-time critical section.
   The CPUID instruction causes VM-exits unconditionally. You should
   detect CPU capability **before** entering a  RT-critical section.
   CPUID can be executed at any privilege level to serialize instruction
   execution and its high efficiency of execution. It's commonly used as a
   serializing instruction in an application by using CPUID
   immediately before and after RDTSC. Remove use of CPUID in this case by
   using RDTSCP instead of RDTSC.  RDTSCP waits until all previous
   instructions have been executed before reading the counter, and the
   subsequent instructions after the RDTSCP normally have data dependency
   on it, so they must wait until the RDTSCP has been executed.

   RDMSR or WRMSR are instructions that cause VM-exits conditionally. On the
   ACRN RTVM, most MSRs are not intercepted by the HV, so they won't cause a
   VM-exit. But there are exceptions for security consideration:

   1) read from APICID and LDR;
   2) write to TSC_ADJUST if VMX_TSC_OFFSET_FULL is zero;
      otherwise, read and write to TSC_ADJUST and TSC_DEADLINE;
   3) write to ICR.

Tip: Do not use RDMSR to access APICID and LDR in an RT critical section.
   ACRN does not present a physical APICID to a guest, so APICID
   and LDR are virtualized even though LAPIC is passthrough. As a result,
   access to APICID and LDR can cause a VM-exit.

Tip: Guarantee that VMX_TSC_OFFSET_FULL is zero; otherwise, do not access TSC_ADJUST and TSC_DEADLINE in the RT critical section.
   ACRN uses VMX_TSC_OFFSET_FULL as the offset between vTSC_ADJUST and
   pTSC_ADJUST. If VMX_TSC_OFFSET_FULL is zero, intercepting
   TSC_ADJUST and TSC_DEADLINE is not necessary. Otherwise, they should be
   intercepted to guarantee functionality.

Tip: Utilize Preempt-RT Linux mechanisms to reduce the access of ICR from the RT core.
   #. Add ``domain`` to ``isolcpus`` ( ``isolcpus=nohz,domain,1`` ) to the kernel parameters.
   #. Add ``idle=poll`` to the kernel parameters.
   #. Add ``rcu_nocb_poll`` along with ``rcu_nocbs=1`` to the kernel parameters.
   #. Disable the logging service like journald, syslogd if possible.

   The parameters shown above are recommended for the guest Preempt-RT
   Linux. For an UP RTVM, ICR interception is not a problem. But for an SMP
   RTVM, IPI may be needed between vCPUs. These tips are about reducing ICR
   access. The example above assumes it is a dual-core RTVM, while core 0
   is a housekeeping core and core 1 is a real-time core. The ``domain``
   flag makes strong isolation of the RT core from the general SMP
   balancing and scheduling algorithms. The parameters ``idle=poll`` and
   ``rcu_nocb_poll`` could prevent the RT core from sending reschedule IPI
   to wakeup tasks on core 0 in most cases. The logging service is disabled
   because an IPI may be issued to the housekeeping core to notify the
   logging service when there are kernel messages output on the RT core.

   .. note::
      If an ICR access is inevitable within the RT critical section, be
      aware of the extra 3~4 microsecond latency for each access.

Tip: Create and initialize the RT tasks at the beginning to avoid runtime access to control registers.
   Accessing Control Registers is another cause of a VM-exit. An ACRN access
   to CR3 and CR8 does not cause a VM-exit. However, writes to CR0 and CR4 may cause a
   VM-exit, which would happen at the spawning and initialization of a new task.

Isolating the impact of neighbor VMs
************************************

ACRN makes use of several technologies and hardware features to avoid
performance impact on the RTVM by neighbor VMs:

Tip: Do not share CPUs allocated to the RTVM with other RT or non-RT VMs.
   ACRN enables CPU sharing to improve the utilization of CPU resources.
   However, for an RT VM, CPUs should be dedicatedly allocated for determinism.

Tip: Use RDT such as CAT and MBA to allocate dedicated resources to the RTVM.
   ACRN enables Intel® Resource Director Technology such as CAT, and MBA
   components such as the GPU via the memory hierarchy. The availability of RDT is
   hardware-specific. Refer to the :ref:`rdt_configuration`.

Tip: Lock the GPU to a feasible lowest frequency.
   A GPU can put a heavy load on the power/memory subsystem. Locking
   the GPU frequency as low as possible can help improve RT performance
   determinism.  GPU frequency can usually be locked in the BIOS, but such
   BIOS support is platform-specific.

Miscellaneous
*************

Tip: Disable timer migration on Preempt-RT Linux.
   Because most tasks are set affinitive to the housekeeping core, the timer
   armed by RT tasks might be migrated to the nearest busy CPU for power
   saving. But it will hurt RT determinism because the timer interrupts raised
   on the housekeeping core need to be resent to the RT core. The timer
   migration can be disabled by the command::

     echo 0 > /proc/kernel/timer_migration

Tip: Add ``mce=off`` to RT VM kernel parameters.
   This parameter disables the mce periodic timer and avoids a VM-exit.

Tip: Disable the Intel processor C-State and P-State of the RTVM.
   Power management of a processor could save power, but it could also impact
   the RT performance because the power state is changing. C-State and P-State
   PM mechanism can be disabled by adding ``processor.max_cstate=0
   intel_idle.max_cstate=0  intel_pstate=disable`` to the kernel parameters.

Tip: Exercise caution when setting ``/proc/sys/kernel/sched_rt_runtime_us``.
   Setting ``/proc/sys/kernel/sched_rt_runtime_us`` to ``-1`` can be a
   problem. A value of ``-1`` allows RT tasks to monopolize a CPU, so that
   a mechanism such as ``nohz`` might get no chance to work, which can hurt
   the RT performance or even (potentially) lock up a system.

Tip: Disable the software workaround for Machine Check Error on Page Size Change.
   By default, the software workaround for Machine Check Error on Page Size
   Change is conditionally applied to the models that may be affected by the
   issue. However, the software workaround has a negative impact on
   performance. If all guest OS kernels are trusted, the
   :option:`CONFIG_MCE_ON_PSC_WORKAROUND_DISABLED` option could be set for performance.

.. note::
   The tips for preempt-RT Linux are mostly applicable to the Linux-based RT OS as well, such as Xenomai.

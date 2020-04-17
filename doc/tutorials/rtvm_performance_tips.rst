.. _rt_perf_tips_rtvm:

ACRN Real-Time VM Performance Tips
##################################

Background
**********

The ACRN real-time VM (RTVM) is a special type of ACRN post-launched VM. In
order to achieve bare metal-like RT performance, a set of constraints and
technologies are applied to the RTVM compared to the ACRN standard VM. With
these additional constraints and technologies, RT tasks can run on the RTVM
without a VM-exit, which is a key virtualization overhead issue.

In addition to the VM-exit, interference from neighbor VMs, such as Service
VMs, Human-Machine-Interface (HMI) VMs, or other RT VMs may affect the
execution of real-time tasks on a certain RTVM. Other technologies are
applied to isolate noise from the neighbor VMs.

Here is the list of key technologies applied to enable the bare metal-like
RT performance:

- LAPIC passthrough with core partitioning.
- PCIe Device Passthrough: Only MSI interrupt-capable PCI devices will be
  supported for the RTVM.
- Enable CAT (Cache Allocation Technology)-based cache isolation: RTVM uses
  a dedicated CLOS (Class of Service). While others may share CLOS, the GPU
  uses a CLOS that will not overlap with the RTVM CLOS.
- PMD virtio: Both virtio BE and FE work in polling mode so that the
  interrupts or notification between the Service VM and RTVM are not needed.
  The RTVM guest memory is hidden from the Service VM except for the virtio
  queue memory which is all that the Service VM can access.

This document list tips that are summarized from issues encountered and
resolved during real-time development and performance tuning.

Mandatory options for an RTVM
*****************************

An RTVM is a post-launched VM with LAPIC passthrough. To launch an ACRN
RTVM, take note of the following options:

**Tip 1:** Apply the acrn-dm option "--lapic_pt" and make the guest RTVM
operate under the LAPIC X2APIC mode to enable the LAPIC passthrough.

The LAPIC passthrough feature of ACRN is configured via the "--lapic_pt"
option, but the feature is actually enabled when LAPIC is switched to X2APIC
mode. So, both conditions should be met to enable an RTVM. The "--rtvm"
option will be automatically attached once "--lapic_pt" is applied.

**Tip 2:** If necessary, use virtio polling mode to prevent the frontend of
the VM-exit from sending a notification to the backend.

We recommend that you passthrough a physical peripheral device to an RTVM,
such as block or an ethernet device. If no physical device is available,
ACRN supports virtio devices and enables the polling mode to avoid a VM-exit
at the frontend. Virtio polling mode can be enabled via the option
"--virtio_poll [polling interval]".

Avoid VM-exit latency
*********************

VM-exit has a significant negative impact on virtualization performance.
A single VM-exit can cause several micro-second latencies, or even longer,
depending on what's done in VMX-root mode. VM-exit is classified into two
types: triggered by external CPU events or triggered by operations initiated
by the vCPU.

ACRN eliminates almost all VM-exits triggered by external events via the
LAPIC passthrough. A few exceptions exist:

- SMI - it will bring the processor into the SMM, causing a much longer
  performance impact. The SMI should be handled in the BIOS.

- NMI - ACRN uses NMI for system-level notification.

Users should take care of VM-exits that are triggered by operations
initiated by the vCPU. Refer to the Intel SMD: "Instructions Cause VM-exits
Unconditionally" (SDM V3, 25.1.2) and "Instructions That Cause VM-exits
Conditionally" (SDM V3, 25.1.3).

**Tip 3:** Do not use CPUID in the RT critical section.

CPUID is an instruction that causes VM-exits unconditionally. As to the
normal usage of CPUID, this can be avoided by detecting the CPU capability
before entering the RT critical section. CPUID can be executed at any
privilege level to serialize instruction execution and its high efficiency
of execution. It's commonly used as a serializing instruction in an
application, and a typical case is using CPUID immediately before and after
RDTSC. In order to remove CPUID in this case, use RDTSCP instead of RDTSC.
Because RDTSCP waits until all previous instructions have been executed
before reading the counter, and the subsequent instructions after the RDTSCP
normally have data dependency on it, they must wait until the RDTSCP has
been executed.

RDMSR or WRMSR are instructions that cause VM-exits conditionally. On the
ACRN RTVM, most MSRs are not intercepted by the HV, so they won't cause a
VM-exit. But there are exceptions for security consideration: 1) read from
APICID and LDR; 2) write to TSC_ADJUST if VMX_TSC_OFFSET_FULL is zero;
otherwise, read and write to TSC_ADJUST and TSC_DEADLINE; 3) write to ICR.

**Tip 4:** Do not use RDMSR to access APICID and LDR at the RT critical
section.

ACRN does not intend to present a physical APICID to a guest so that APICID
and LDR are virtualized even though LAPIC is passthrough. As a result,
access to APICID and LDR can cause a VM-exit.

**Tip 5:** Guarantee that VMX_TSC_OFFSET_FULL is zero; otherwise, do not
access TSC_ADJUST and TSC_DEADLINE in the RT critical section.

ACRN uses VMX_TSC_OFFSET_FULL as the offset between vTSC_ADJUST and
pTSC_ADJUST; therefore, if VMX_TSC_OFFSET_FULL is zero, intercepting
TSC_ADJUST and TSC_DEADLINE is not necessary. Otherwise, they should be
intercepted to guarantee functionality.

**Tip 6:** Utilize Preempt-RT Linux mechanisms to reduce the access of ICR
from the RT core:

#. Add "domain" to the "isolcpus" ( “isolcpus=nohz,domain,1” ) to the kernel parameters.
#. Add "idle=poll" to the kernel parameters.
#. Add "rcu_nocb_poll" along with "rcu_nocbs=1" to the kernel parameters.
#. Disable the logging service like journald, syslogd if possible.

These parameters are recommended for the guest Preempt-RT Linux. For a UP
RTVM, ICR interception is not a problem. But for an SMP RTVM, IPI may be
needed between vCPUs; these tips are about to reduce the ICR access. The
example above assumes it is a dual-core RTVM, while core 0 is a housekeeping
core and core 1 is a real-time core. The "domain" flag makes strong
isolation of the RT core from the general SMP balancing and scheduling
algorithms. "idle=poll" and "rcu_nocb_poll" could prevent the RT core from
sending reschedule IPI to wakeup tasks on core 0 in most cases. And the
disabling of the logging service is because an IPI may be issued to the
housekeeping core to notify the logging service when there are kernel
messages output on the RT core.

.. note::
   If an ICR access is inevitable within the RT critical section, please be
   aware of the extra 3~4 us latency from each access.

**TIP 7:** Create and initialize the RT tasks at the beginning to avoid
runtime access to control registers.

The access to Control Registers is another cause of a VM-exit. An ACRN access
to CR3 and CR8 do not cause a VM-exit, but writes to CR0 and CR4 may cause a
VM-exit, which would happen at the spawning and initialization of a new task.

Isolating the impact of neighbor VMs
************************************

ACRN makes use of several technologies and hardware features to avoid the
impact to the RTVM from neighbor VMs:

**TIP 8:** Do not share CPUs allocated to the RTVM with other RT/non-RT VMs.

ACRN enables CPU sharing to improve the utilization of CPU resources.
However, for RT VM, CPUs should be dedicatedly allocated for the determinism.

**TIP 9:** Use RDT such as CAT and MBA to allocate dedicated resources to
the RTVM.

ACRN enables the Intel® Resource Director Technology, such as CAT and MBA,
components such as the GPU via memory hierarchy. The availability of RDT is
hardware-specific. Refer to the :ref:`rdt_configuration`.

**TIP 10:** Lock the GPU to a feasible lowest frequency.

GPU can put heavy pressure on the power/memory subsystem, so locking the GPU
frequency as low as possible can help to improve the determinism of RT
performance. It can be locked in the BIOS, but the availability of certain
BIOS option is platform-specific.

Miscellaneous
*************

**TIP 11:**  Disable timer migration on Preempt-RT Linux.

Because most tasks are set affinitive to the housekeeping core, the timer
armed by RT tasks might be migrated to the nearest busy CPU for power
saving. But it will hurt the determinism because the timer interrupts raised
on the housekeeping core need to be resent to the RT core. The timer
migration could be disabled by cmd: "echo 0 > /proc/kernel/timer_migration"

**TIP 12:** Add "mce=off" to RT VM kernel parameters.

"mce=off" can disable the mce periodic timer in order to void a VM-exit.

**TIP 13:** Disable the Intel processor C-State and P-State of the RTVM.

Power management of a processor could save power, but it could also impact
the RT performance because the power state is changing. C-State and P-State
PM mechanism can be disabled by adding "processor.max_cstate=0
intel_idle.max_cstate=0  intel_pstate=disabled" to the kernel parameters.

**TIP 14:** Exercise caution when setting /proc/sys/kernel/sched_rt_runtime_us.

Setting /proc/sys/kernel/sched_rt_runtime_us to -1 can be dangerous. A value
of -1 allows RT tasks to monopolize a CPU, so that the mechanism such as
"nohz" might get no chance to work, which can hurt the RT performance or
even (potentially) lock up a system.

**TIP 15:** Disable the software workaround for Machine Check Error on Page
Size Change.

By default, the software workaround for Machine Check Error on Page Size
Change is conditionally applied to the models that may be affected by the
issue. However, the software workaround has a negative impact on
performance. If all guest OS kernels are trusted, the
:option:`CONFIG_MCE_ON_PSC_WORKAROUND_DISABLED` option could be set for performance.

.. note::
   The tips for preempt-RT Linux is mostly applicable to the Linux-based RT OS as well, such as Xenomai.


.. _cpu_sharing:

ACRN CPU Sharing
################

Introduction
************

The goal of CPU Sharing is to fully utilize the physical CPU resource to
support more virtual machines. Currently, ACRN only supports 1 to 1
mapping mode between virtual CPUs (vCPUs) and physical CPUs (pCPUs).
Because of the lack of CPU sharing ability, the number of VMs is
limited. To support CPU Sharing, we have introduced a scheduling
framework and implemented two simple small scheduling algorithms to
satisfy embedded device requirements. Note that, CPU Sharing is not
available for VMs with local APIC passthrough (``--lapic_pt`` option).

Scheduling Framework
********************

To satisfy the modularization design concept, the scheduling framework
layer isolates the vCPU layer and scheduler algorithm. It does not have
a vCPU concept so it is only aware of the thread object instance. The
thread object state machine is maintained in the framework. The
framework abstracts the scheduler algorithm object, so this architecture
can easily extend to new scheduler algorithms.

.. figure:: images/cpu_sharing_framework.png
   :align: center

The below diagram shows that the vCPU layer invokes APIs provided by
scheduling framework for vCPU scheduling. The scheduling framework also
provides some APIs for schedulers. The scheduler mainly implements some
callbacks in an ``acrn_scheduler`` instance for scheduling framework.
Scheduling initialization is invoked in the hardware management layer.

.. figure:: images/cpu_sharing_api.png
   :align: center

vCPU affinity
*************

Currently, we do not support vCPU migration; the assignment of vCPU
mapping to pCPU is statically configured by acrn-dm through
``--cpu_affinity``. Use these rules to configure the vCPU affinity:

- Only one bit can be set for each affinity item of vCPU.
- vCPUs in the same VM cannot be assigned to the same pCPU.

Here is an example for affinity:

- VM0: 2 vCPUs, pinned to pCPU0 and pCPU1
- VM1: 2 vCPUs, pinned to pCPU2 and pCPU3
- VM2: 2 vCPUs, pinned to pCPU0 and pCPU1

.. figure:: images/cpu_sharing_affinity.png
   :align: center

Thread object state
*******************

The thread object contains three states: RUNNING, RUNNABLE, and BLOCKED.

.. figure:: images/cpu_sharing_state.png
   :align: center

After a new vCPU is created, the corresponding thread object is
initiated. The vCPU layer invokes a wakeup operation. After wakeup, the
state for the new thread object is set to RUNNABLE, and then follows its
algorithm to determine whether or not to preempt the current running
thread object. If yes, it turns to the RUNNING state. In RUNNING state,
the thread object may turn back to the RUNNABLE state when it runs out
of its timeslice, or it might yield the pCPU by itself, or be preempted.
The thread object under RUNNING state may trigger sleep to transfer to
BLOCKED state.

Scheduler
*********

The below block diagram shows the basic concept for the scheduler. There
are two kinds of schedulers in the diagram: NOOP (No-Operation) scheduler
and BVT (Borrowed Virtual Time) scheduler.


- **No-Operation scheduler**:

  The NOOP (No-operation) scheduler has the same policy as the original
  1-1 mapping previously used; every pCPU can run only two thread objects:
  one is the idle thread, and another is the thread of the assigned vCPU.
  With this scheduler, vCPU works in Work-Conserving mode, which always
  try to keep resource busy, and will run once it is ready. Idle thread
  can run when the vCPU thread is blocked.

- **Borrowed Virtual Time scheduler**:

  BVT (Borrowed Virtual time) is a virtual time based scheduling
  algorithm, it dispatching the runnable thread with the earliest
  effective virtual time.

  TODO: BVT scheduler will be built on top of prioritized scheduling
  mechanism, i.e. higher priority threads get scheduled first, and same
  priority tasks are scheduled per BVT.

  - **Virtual time**: The thread with the earliest effective virtual
    time (EVT) is dispatched first.
  - **Warp**: a latency-sensitive thread is allowed to warp back in
    virtual time to make it appear earlier. It borrows virtual time from
    its future CPU allocation and thus does not disrupt long-term CPU
    sharing
  - **MCU**: minimum charging unit, the scheduler account for running time
    in units of MCU.
  - **Weighted fair sharing**: each runnable thread receives a share of
    the processor in proportion to its weight over a scheduling
    window of some number of MCU.
  - **C**: context switch allowance.  Real time by which the current
    thread is allowed to advance beyond another runnable thread with
    equal claim on the CPU. C is similar to the quantum in conventional
    timesharing.


Scheduler configuration
***********************

Two places in the code decide the usage for the scheduler.

* The option in Kconfig decides the only scheduler used in runtime.
  ``hypervisor/arch/x86/Kconfig``

.. code-block:: none

  config SCHED_BVT
          bool "BVT scheduler"
          help
          BVT (Borrowed Virtual time) is virtual time based scheduling algorithm. It
          dispatches the runnable thread with the earliest effective virtual time.
          TODO: BVT scheduler will be built on top of prioritized scheduling mechanism,
          i.e. higher priority threads get scheduled first, and same priority tasks are
          scheduled per BVT.

The default scheduler is **SCHED_NOOP**. To use the BVT, change it to
**SCHED_BVT** in the **ACRN Scheduler**.

* The cpu_affinity is configured by acrn-dm command.

  For example, assign physical CPUs (pCPUs) 1 and 3 to this VM using::

    --cpu_affinity 1,3


Example
*******

Use the following settings to support this configuration in the industry scenario:

+---------+-------+-------+-------+
|pCPU0    |pCPU1  |pCPU2  |pCPU3  |
+=========+=======+=======+=======+
|SOS + WaaG       |RT Linux       |
+-----------------+---------------+

- offline pcpu2-3 in SOS.

- launch guests.

  - launch WaaG with "--cpu_affinity=0,1"
  - launch RT with "--cpu_affinity=2,3"


After you start all VMs, check the vCPU affinities from the Hypervisor
console with the ``vcpu_list`` command:

.. code-block:: console

   ACRN:\>vcpu_list

   VM ID    PCPU ID    VCPU ID    VCPU ROLE    VCPU STATE    THREAD STATE
   =====    =======    =======    =========    ==========    ==========
     0         0          0       PRIMARY      Running          BLOCKED
     0         1          0       SECONDARY    Running          BLOCKED
     1         0          0       PRIMARY      Running          RUNNING
     1         1          0       SECONDARY    Running          RUNNING
     2         2          0       PRIMARY      Running          RUNNING
     2         3          1       SECONDARY    Running          RUNNING

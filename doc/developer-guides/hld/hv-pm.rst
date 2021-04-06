.. _pm_hld:

Power Management
################

System PM Module
****************

The PM module in the hypervisor does three things:

-  Monitors all guests power state transitions and emulates a low power
   state for the guests which are launched by the HV directly.

-  Once all guests enter low power state, the Hypervisor handles its
   own low-power state transition.

-  Once the system resumes from low-power mode, the hypervisor handles its
   own resume and emulates the Service VM resume.

It is assumed that the Service VM does not trigger any power state transition
until the VM manager of ACRN notifies it that all User VMs are inactive
and Service VM offlines all its virtual APs. And it is assumed that HV
does not trigger its own power state transition until all guests are in
low power state.

:numref:`pm-low-power-transition` shows the Hypervisor entering S3
state process. The Service VM triggers power state transition by
writing ACPI control register on its virtual BSP (which is pinned to the
physical BSP). The hypervisor then does the following in sequence before
it writes to the physical ACPI control register to trigger physical
power state transition:

-  Pauses Service VM.
-  Wait all other guests enter low power state.
-  Offlines all physical APs.
-  Save the context of console, ioapic of Service VM, I/O MMU, lapic of
   Service VM, virtual BSP.
-  Save the context of physical BSP.

When exiting from S3 state, the hypervisor does similar steps in
reverse order to restore contexts, start APs and resume all guests launched
by hypervisor directly. Service VM is responsible for starting its own
virtual APs as well as User VMs.

.. figure:: images/pm-image24-105.png
   :align: center
   :name: pm-low-power-transition

   Service VM/Hypervisor S3 transition process

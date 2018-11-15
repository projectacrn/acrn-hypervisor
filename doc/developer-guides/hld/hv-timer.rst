.. _timer-hld:

Timer
#####

Because ACRN is a flexible, lightweight reference hypervisor, we provide
limited timer management services:

- Only lapic tsc-deadline timer is supported as the clock source.

- A timer can only be added on the logical CPU for a process or thread. Timer
  scheduling or timer migrating are not supported.

How it works
************

When the system boots, we check that the hardware supports lapic
tsc-deadline timer by checking CPUID.01H:ECX.TSC_Deadline[bit 24]. If
support is missing, we output an error message and panic the hypervisor.
If supported, we register the timer interrupt callback that raises a
timer softirq on each logical CPU and set the lapic timer mode to
tsc-deadline timer mode by writing the local APIC LVT register.

Data Structures and APIs
************************

Interfaces Design
=================

.. doxygenfunction:: initialize_timer
   :project: Project ACRN

.. doxygenfunction:: timer_expired
   :project: Project ACRN

.. doxygenfunction:: add_timer
   :project: Project ACRN

.. doxygenfunction:: del_timer
   :project: Project ACRN

.. doxygenfunction:: timer_init
   :project: Project ACRN

.. doxygenfunction:: check_tsc
   :project: Project ACRN

.. doxygenfunction:: calibrate_tsc
   :project: Project ACRN


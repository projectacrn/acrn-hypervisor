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

.. note:: API link to hv_timer and per_cpu_timer structs in include/arch/x86/timer.h
   And to the function APIs there too.

Before adding a timer, we must initialize the timer with
*initialize_timer*.  The processor generates a timer interrupt when the
value of timer-stamp counter is greater than or equal to the *fire_tsc*
field. If you want to add a periodic timer, you should also pass the
period (unit in tsc cycles), otherwise, period_in_cycle will be ignored.
When the timer interrupt is generated, it will call the callback
function *func* with parameter *priv_data*.

The *initialize_timer* function only initialize the timer data
structure; it will not program the ``IA32_TSC_DEADLINE_MSR`` to generate
the timer interrupt. If you want to generate a timer interrupt, you must
call *add_timer* to add the timer to the *per_cpu_timer* timer_list. In
return, we will chose the nearest expired timer on the timer_list and
program ``IA32_TSC_DEADLINE_MSR`` by writing its value to fire_ts. Then
when the fire_tsc expires, it raises the interrupt whose callback raises
a softirq. We will handle the software interrupt before the VM reenters
the guest. (Currently, the hypervisor only uses the timer for the
console).

The timer softirq handler will check each expired timer on its
timer_list.  Before calling the expired timer callback handler, it will
remove the timer from its logical cpu timer_list. After calling the
timer callback handler, it will re-add the timer to the timer_list if
it's a periodic timer. If you want to modify a timer before it expires,
you should call del_timer to remove the timer from the timer_list, then
call add_timer again after updating the timer fields.

.. note::

   Only call initialize_timer only once for each timer.
   Don't call add_timer or del_timer in the timer callback function.

.. _timer-hld:

Timer Management high-level design
###################################

This document describes timer management for the ACRN hypervisor.

Overview
********

Timer management support is limited to only use LAPIC tsc-deadline timer
as the clock source. Also, you can only add a timer on the logical CPU
for the process or thread.  Timer scheduling and timer migrating are
forbidden.

How it works
************

When the system boots, we check that the hardware supports a LAPIC
tsc-deadline timer by checking ``CPUID.01H:ECX.TSC_Deadline[bit 24]``.
If support is missing, we output an error message and panic the
hypervisor.  Otherwise, we register the timer interrupt callback that
raises a timer software interrupt on each logical CPU and set the lapic
timer mode to tsc-deadline timer mode by writing the local APIC LVT
register.

timer.h API
***********

The timer data structures are defined in
``hypervisor/include/arch/x86/timer.h``:

.. code-block:: c

   struct per_cpu_timers {
      struct list_head timer_list;    /* active timer list */
   };

   struct hv_timer {
      struct list_head node;      /* link-list for all timers */
      int mode;                   /* timer mode: one-shot or periodic */
      uint64_t fire_tsc;          /* tsc deadline to interrupt */
      uint64_t period_in_cycle;   /* period of the periodic timer in units of TSC cycles */
      timer_handle_t func;        /* callback when time reached */
      void *priv_data;            /* callback function private data */
   };

There are three external timer APIs (also defined in ``timer.h``):

.. code-block:: c

   static inline void initialize_timer(
      struct hv_timer *timer,
      timer_handle_t func,
      void *priv_data,
      uint64_t fire_tsc,
      int mode,
      uint64_t period_in_cycle);

   int add_timer(struct hv_timer *timer);

   void del_timer(struct hv_timer *timer);

Before adding a timer, we must initialize the timer by calling
``initialize_timer``. The processor generates a timer interrupt when the
value of timer-stamp counter is greater than or equal to the
``fire_tsc``.  If you want to add a periodic timer, you should also pass
the period (in tsc cycles), otherwise, the period_in_cycle will be
ignored. When the timer interrupt occurs, it will call the callback
function func with parameter priv_data.

The ``initialize_timer`` function only initializes the timer data
structure.  It will not program the ``IA32_TSC_DEADLINE_MSR`` to
generate the timer interrupt. If you want to generate a timer interrupt,
you must call ``add_timer`` to add the timer to the ``per_cpu_timer``
``timer_list``.

The hypervisor will choose the nearest expired timer on the timer_list
and program ``IA32_TSC_DEADLINE_MSR`` by writing its value to fire_ts.
When the fire_tsc expires, it raises an interrupt whose callback raises a
software interrupt. The hypervisor handles the software interrupt before
the VM reenters the guest. (Currently, the hypervisor doesn't use the
timer except the console). The timer software interrupt handler will
check each expired timer on its timer_list. Before calling the expired
timer callback handler, it will remove the timer from its logical CPU
timer_list.  After calling the timer callback handler, it will re-add
the timer to the timer_list if it's a periodic timer.

If you want to modify a timer before it expires, you should call
``del_timer`` to remove the timer from the timer_list, update the timer
fields, then call ``add_timer`` to put the timer back on the
timer_list.

.. note::
   Some important notes about the timer API use:

   * Only call ``initialize_timer`` once for each timer.
   * Don't call ``add_timer`` or ``del_timer`` in the timer callback function.

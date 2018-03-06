.. _hypervisor_primer:

Hypervisor Developer Primer
###########################

This Developer Primer introduces the fundamental components and
virtualization technology used by this open source reference hypervisor
stack. Code level documentation and additional details can be found by
consulting the :ref:`hypercall_apis` documentation and the source code
in GitHub.

The Hypervisor acts as a host with full control of the processor(s) and
the hardware (physical memory, interrupt management and I/O). It
provides the Guest OS with an abstraction of a virtual processor,
allowing the guest to think it is executing directly on a logical
processor.

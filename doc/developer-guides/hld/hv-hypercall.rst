.. _hv-hypercall:

Hypercall / HSM Upcall
######################

The hypercall/upcall is used to request services between the guest VM and the
hypervisor. The hypercall is from the guest VM to the hypervisor, and the upcall
is from the hypervisor to the guest VM. The hypervisor supports
hypercall APIs for VM management, I/O request distribution, interrupt injection,
PCI assignment, guest memory mapping, power management, and secure world switch.

There are some restrictions for hypercall and upcall:

#. Only specific VMs (the Service VM and the VM with Trusty enabled)
   can invoke hypercalls. A VM that cannot invoke hypercalls gets ``#UD``
   (invalid opcode exception).
#. Only ring 0 hypercalls from the guest VM are handled by the hypervisor;
   otherwise, the hypervisor injects ``#GP(0)`` (generation protection
   exception with error code ``0``) to the guest VM.
#. Each VM is permitted to invoke a certain subset of hypercalls. A VM
   with Trusty enabled is allowed to invoke Trusty hypercalls, and the Service
   VM is allowed to invoke the other hypercalls. A VM that invokes an
   unpermitted hypercall gets the return value ``-EINVAL``.
   See :ref:`secure-hypervisor-interface` for a detailed description.
#. The hypervisor needs to protect the critical resources such as global VM and
   vCPU structures for VM and vCPU management hypercalls.
#. Upcall is only used for the Service VM.


The hypervisor and Service VM both use the same vector (0xF3), which is reserved
as the x86 platform IPI vector for hypervisor notification to the Service VM.
This upcall is necessary whenever there is a device emulation requirement to the
Service VM. The upcall vector (0xF3) is injected to Service VM vCPU0. The
Service VM registers the IRQ handler for vector (0xF3) and notifies the I/O
emulation module in the Service VM once the IRQ is triggered. View the detailed
upcall process at :ref:`ipi-management`.

Hypercall APIs Reference
************************

:ref:`hypercall_apis` for the Service VM

:ref:`trusty-hypercalls` for Trusty



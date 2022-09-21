.. _hv-hypercall:

Hypercall / HSM Upcall
######################

The hypercall/upcall is used to request services between the guest VM and the
hypervisor. The hypercall is from the guest VM to the hypervisor, and the upcall
is from the hypervisor to the guest VM. The hypervisor supports
hypercall APIs for VM management, I/O request distribution, interrupt injection,
PCI assignment, guest memory mapping, power management, and secure world switch.

The application binary interface (ABI) of ACRN hypercalls is defined as follows.

- A guest VM executes the ``vmcall`` instruction to trigger a hypercall.

- Input parameters of a hypercall include:

  - A hypercall ID in register ``R8``, which specifies the kind of service
    requested by the guest VM.

  - The first parameter in register ``RDI`` and the second in register
    ``RSI``. The semantics of those two parameters vary among different kinds of
    hypercalls and are defined in the :ref:`hv-hypercall-ref`. For hypercalls
    requesting operations on a specific VM, the first parameter is typically the
    ID of that VM.

- The register ``RAX`` contains the return value of the hypercall after a guest
  VM executes the ``vmcall`` instruction, unless the ``vmcall`` instruction
  triggers an exception. Other general-purpose registers are not modified by a
  hypercall.

- If a hypercall parameter is defined as a pointer to a data structure,
  fields in that structure can be either input, output, or inout.

There are some restrictions for hypercalls and upcalls:

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

.. _hv-hypercall-ref:

Hypercall APIs Reference
************************

:ref:`hypercall_apis` for the Service VM

:ref:`trusty-hypercalls` for Trusty



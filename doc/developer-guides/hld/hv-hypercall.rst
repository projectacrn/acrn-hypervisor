.. _hv-hypercall:

Hypercall / VHM upcall
######################

The hypercall/upcall is used to request services between the Guest VM and the hypervisor.
The hypercall is from the Guest VM to hypervisor, and the upcall is from the hypervisor to the Guest VM.
The hypervisor currently supports hypercall APIs for VM management, I/O request
distribution, interrupt injection, PCI assignment, guest memory mapping,
power management, and secure world switch.

There are some restrictions for hypercall and upcall:

#. Only ring 0 hypercalls from the guest VM are handled by the hypervisor;
   otherwise, the hypervisor will inject GP to the Guest VM.
#. All the hypercalls (except secure world hypercalls) must be called from the Service VM;
   otherwise, the hypervisor will inject UD to the Guest VM.
   see :ref:`secure-hypervisor-interface` for a detailed description.
#. The hypervisor needs to protect the critical resources such as global VM and VCPU structures
   for VM and VCPU management hypercalls.
#. Upcall is only used for the Service VM.


HV and Service VM both use the same vector (0xF3) reserved as x86 platform
IPI vector for HV notification to the Service VM. This upcall is necessary whenever
there is device emulation requirement to the Service VM. The upcall vector (0xF3) is
injected to Service VM vCPU0. The Service VM will register the IRQ handler for vector (0xF3) and notify the I/O emulation
module in the Service VM once the IRQ is triggered.
View the detailed upcall process at :ref:`ipi-management`

Hypercall APIs reference:
*************************

:ref:`hypercall_apis` for the Service VM

:ref:`trusty-hypercalls` for Trusty



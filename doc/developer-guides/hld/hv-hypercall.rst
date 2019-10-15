.. _hv-hypercall:

Hypercall / VHM upcall
######################

Hypercall/upcall used to request services between Guest VM and hypervisor,
hypercall is from Guest VM to hypervisor, upcall is from hypervisor to Guest VM.
Hypervisor currently supports hypercall APIs for VM management, I/O request
distribution,interrupt injection, PCI assignment, guest memory mapping,
power management and secure world switch.

There are some restrictions for hypercall and upcall:

#. Only ring0 hypercalls from the guest VM are handled by the hypervisor,
   otherwise hypervisor will inject GP to Guest VM.
#. All the hypercalls (except secure world hypercalls) must be called from SOS VM,
   otherwise hypervisor will inject UD to Guest VM.
   see :ref:`secure-hypervisor-interface` for a detailed description.
#. Hypervisor needs to protect the critical resources such as global VM and VCPU structures
   for VM and VCPU management hypercalls.
#. Upcall is only used for SOS VM.


HV and Service OS (SOS) both use the same vector(0xF3) reserved as x86 platform
IPI vector for HV notification to SOS. This upcall is necessary whenever
there is device emulation requirement to SOS. The upcall vector(0xF3) is
injected to SOS vCPU0.SOS will register the irq handler for vector(0xF3) and notify the I/O emulation
module in SOS once the irq is triggered.
The detailed upcall process see :ref:`ipi-management`

Hypercall APIs reference:
*************************

:ref:`hypercall_apis` for SOS

:ref:`trusty-hypercalls` for Trusty



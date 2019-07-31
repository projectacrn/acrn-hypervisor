.. _hv-hypercall:

Hypercall / VHM upcall
######################

HV currently supports hypercall APIs for VM management, I/O request
distribution, and guest memory mapping.

HV and Service OS (SOS) also use vector 0xF7, reserved as x86 platform
IPI vector for HV notification to SOS. This upcall is necessary whenever
there is device emulation requirement to SOS. The upcall vector 0xF7 is
injected to SOS vCPU0

SOS will register the irq handler for 0xF7 and notify the I/O emulation
module in SOS once the irq is triggered.


.. note:: Add API doc references for General interface, VM management
   interface, IRQ and Interrupts, Device Model IO request distribution,
   Guest memory management, PCI assignment and IOMMU, Debug, Trusty, Power
   management

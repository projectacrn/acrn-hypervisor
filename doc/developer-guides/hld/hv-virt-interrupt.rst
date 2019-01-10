.. _virtual-interrupt-hld:

Virtual Interrupt
#################

This section introduces ACRN guest virtual interrupt
management, which includes:

- VCPU request for virtual interrupt kick off,
- vPIC/vIOAPIC/vLAPIC for virtual interrupt injection interfaces,
- physical-to-virtual interrupt mapping for a pass-thru device, and
- the process of VMX interrupt/exception injection.

A guest VM never owns any physical interrupts. All interrupts received by
Guest OS come from a virtual interrupt injected by vLAPIC, vIOAPIC or
vPIC. Such virtual interrupts are triggered either from a pass-through
device or from I/O mediators in SOS via hypercalls. Section 3.8.6
introduces how the hypervisor manages the mapping between physical and
virtual interrupts for pass-through devices.

Emulation for devices is inside SOS user space device model, i.e.,
acrn-dm. However for performance consideration: vLAPIC, vIOAPIC, and vPIC
are emulated inside HV directly.

From guest OS point of view, vPIC is Virtual Wire Mode via vIOAPIC. The
symmetric I/O Mode is shown in :numref:`pending-virt-interrupt` later in
this section.

The following command line
options to guest Linux affects whether it uses PIC or IOAPIC:

-  **Kernel boot param with vPIC**: add "maxcpu=0" Guest OS will use PIC
-  **Kernel boot param with vIOAPIC**: add "maxcpu=1" (as long as not "0")
   Guest OS will use IOAPIC. And Keep IOAPIC pin2 as source of PIC.

vCPU Request for Interrupt Injection
************************************

The vCPU request mechanism (described in :ref:`pending-request-handlers`) is leveraged
to inject interrupts to a certain vCPU. As mentioned in
:ref:`ipi-management`,
physical vector 0xF0 is used to kick VCPU out of its VMX non-root mode,
used to make a request for virtual interrupt injection or other
requests such as flush EPT.

The eventid supported for virtual interrupt injection includes:

.. code-block:: c

   #define ACRN_REQUEST_EXCP   0  /* request for exception injection */
   #define ACRN_REQUEST_EVENT  1  /* vLAPIC event */
   #define ACRN_REQUEST_EXTINT 2  /* external interrupt from vPIC */
   #define ACRN_REQUEST_NMI    3  /* non-maskable interrupt */


The *vcpu_make_request* is necessary for a virtual interrupt
injection. If the target vCPU is running under VMX non-root mode, it
will send an IPI to kick it out, which leads to an external-interrupt
VM-Exit. For some cases there is no need to send IPI when making a request,
because the CPU making the request itself is the target VCPU. For
example, the #GP exception request always happens on the current CPU when it
finds an invalid emulation has happened. An external interrupt for a pass-thru
device always happens on the VCPUs this device belonging to, so after it
triggers an external-interrupt VM-Exit, the current CPU is also the
target VCPU.

Virtual LAPIC
*************

LAPIC is virtualized for all Guest types: SOS and UOS. Given support by
the
physical processor, APICv Virtual Interrupt Delivery (VID) is enabled
and will support Posted-Interrupt feature. Otherwise, it will fall back to legacy
virtual interrupt injection mode.

vLAPIC provides the same features as the native LAPIC:

-  Vector mask/unmask
-  Virtual vector injections (Level or Edge trigger mode) to vCPU
-  vIOAPIC notification of EOI processing
-  TSC Timer service
-  vLAPIC support CR8 to update TPR
-  INIT/STARTUP handling

vLAPIC APIs
===========

APIs are provided when an interrupt source from vLAPIC needs to inject
an interrupt, for example:

- from LVT like LAPIC timer
- from vIOAPIC for a pass-thru device interrupt
- from an emulated device for a MSI

These APIs will finish by making a request for *ACRN_REQUEST_EVENT.*

.. doxygenfunction:: vlapic_intr_accepted
  :project: Project ACRN

.. doxygenfunction:: vlapic_pending_intr
  :project: Project ACRN

.. doxygenfunction:: vlapic_set_local_intr
  :project: Project ACRN

.. doxygenfunction:: vlapic_intr_msi
  :project: Project ACRN

.. doxygenfunction:: vlapic_post_intr
  :project: Project ACRN

.. doxygenfunction:: apicv_get_pir_desc_paddr
  :project: Project ACRN

EOI processing
==============

EOI virtualization is enabled if APICv virtual interrupt delivery is
supported. Except for level triggered interrupts, VM will not exit in
case of EOI.

In case of no APICv virtual interrupt delivery support, vLAPIC requires
EOI from Guest OS whenever a vector was acknowledged and processed by
guest. vLAPIC behavior is the same as HW LAPIC. Once an EOI is received,
it clears the highest priority vector in ISR and TMR, and updates PPR
status. vLAPIC will then notify vIOAPIC if the corresponding vector
comes from vIOAPIC. This only occurs for the level triggered interrupts.

Virtual IOAPIC
**************

vIOAPIC is emulated by HV when Guest accesses MMIO GPA range:
0xFEC00000-0xFEC01000. vIOAPIC for SOS should match to the native HW
IOAPIC Pin numbers. vIOAPIC for UOS provides 48 Pins. As the vIOAPIC is
always associated with vLAPIC, the virtual interrupt injection from
vIOAPIC will finally trigger a request for vLAPIC event by calling
vLAPIC APIs.

**Supported APIs:**

.. doxygenfunction:: vioapic_set_irqline_lock
  :project: Project ACRN

.. doxygenfunction:: vioapic_set_irqline_nolock
  :project: Project ACRN

Virtual PIC
***********

vPIC is required for TSC calculation. Normally UOS will boot with
vIOAPIC and vPIC as the source of external interrupts to Guest. On every
VM Exit, HV will check if there are any pending external PIC interrupts.
vPIC APIs usage are similar to vIOAPIC.

ACRN hypervisor emulates a vPIC for each VM based on IO range 0x20~0x21,
0xa0~0xa1 and 0x4d0~0x4d1.

If an interrupt source from vPIC need to inject an interrupt, the
following APIs need be called, which will finally make a request for
*ACRN_REQUEST_EXTINT or ACRN_REQUEST_EVENT*:

.. doxygenfunction:: vpic_set_irqline
  :project: Project ACRN

The following APIs are used to query the vector needed to be injected and ACK
the service (means move the interrupt from request service - IRR to in
service - ISR):

.. doxygenfunction:: vpic_pending_intr
  :project: Project ACRN

.. doxygenfunction:: vpic_intr_accepted
  :project: Project ACRN

Virtual Exception
*****************

When doing emulation, an exception may need to be triggered in
hypervisor, for example:

- if guest accesses an invalid vMSR register,
- hypervisor needs to inject a #GP, or 
- during instruction emulation, an instruction fetch may access
  a non-exist page from rip_gva, at that time a #PF need be injected.

ACRN hypervisor implements virtual exception injection using these APIs:

.. doxygenfunction:: vcpu_queue_exception
  :project: Project ACRN

.. doxygenfunction:: vcpu_inject_extint
  :project: Project ACRN

.. doxygenfunction:: vcpu_inject_nmi
  :project: Project ACRN

.. doxygenfunction:: vcpu_inject_gp
  :project: Project ACRN

.. doxygenfunction:: vcpu_inject_pf
  :project: Project ACRN

.. doxygenfunction:: vcpu_inject_ud
  :project: Project ACRN

.. doxygenfunction:: vcpu_inject_ac
  :project: Project ACRN

.. doxygenfunction:: vcpu_inject_ss
  :project: Project ACRN

ACRN hypervisor uses the *vcpu_inject_gp/vcpu_inject_pf* functions
to queue exception request, and follows SDM vol3 - 6.15, Table 6-5 to
generate double fault if the condition is met.

Virtual Interrupt Injection
***************************

The source of virtual interrupts comes from either DM or assigned
devices.

-  **For SOS assigned devices**: as all devices are assigned to SOS
   directly. Whenever there is a device's physical interrupt, the
   corresponding virtual interrupts are injected to SOS via
   vLAPIC/vIOAPIC. SOS does not use vPIC and does not have emulated
   devices. See section 3.8.5 Device assignment.

-  **For UOS assigned devices**: only PCI devices could be assigned to
   UOS. Virtual interrupt injection follows the same way as SOS. A
   virtual interrupt injection operation is triggered when a
   device's physical interrupt occurs.

-  **For UOS emulated devices**: DM (acrn-dm) is responsible for UOS
   emulated devices' interrupt lifecycle management. DM knows when
   an emulated device needs to assert a virtual IOPAIC/PIC Pin or
   needs to send a virtual MSI vector to Guest. These logic is
   entirely handled by DM.

.. figure:: images/virtint-image64.png
   :align: center
   :name: pending-virt-interrupt

   Handle pending virtual interrupt

Before APICv virtual interrupt delivery, a virtual interrupt can be
injected only if guest interrupt is allowed. There are many cases
that Guest ``RFLAGS.IF`` gets cleared and it would not accept any further
interrupts. HV will check for the available Guest IRQ windows before
injection.

NMI is unmasked interrupt and its injection is always allowed
regardless of the guest IRQ window status. If current IRQ
windows is not present, HV would enable
``MSR_IA32_VMX_PROCBASED_CTLS_IRQ_WIN (PROCBASED_CTRL.bit[2])`` and
VM Enter directly. The injection will be done on next VM Exit once Guest
issues ``STI (GuestRFLAG.IF=1)``.

Data structures and interfaces
******************************

There is no data structure exported to the other components in the
hypervisor for virtual interrupts. The APIs listed in the previous
sections are meant to be called whenever a virtual interrupt should be
injected or acknowledged.

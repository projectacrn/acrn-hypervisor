.. _hv-vm-management:

VM Management
#############

The ACRN hypervisor maintained a dedicated VM structure instance for each
running VM, and a series VM APIs like create_vm, start_vm, reset_vm, shutdown_vm
etc are used to switch a VM to the right state, according to the requirements of
applications or system power operations.

VM structure
************

The ``acrn_vm`` structure is defined to manage a VM instance, this structure
maintained a VM's HW resources like vcpu, vpic, vioapic, vuart, vpci. And at
the same time ``acrn_vm`` structure also recorded a bunch of SW information
related with corresponding VM, like info for VM identifier, info for SW
loader, info for memory e820 entries, info for IO/MMIO handlers, info for
platform level cpuid entries, and so on.

The ``acrn_vm`` structure instance will be created by create_vm API, and then
work as the first parameter for other VM APIs.

VM state
********

Generally, a VM is not running at the beginning: it is in a 'powered off'
state. After it got created successfully, the VM enter a 'created' state.
Then the VM could be kick to run, and enter a 'started' state. When the
VM powers off, the VM returns to a 'powered off' state again.
A VM can be paused to wait some operation when it is running, so there is
also a 'paused' state.

:numref:`hvvm-state` illustrates the state-machine of a VM state transition,
please refer to :ref:`hv-cpu-virt` for related VCPU state.

.. figure:: images/hld-image108.png
   :align: center
   :name: hvvm-state

   Hypervisor VM State Transition

VM State Management
*******************

Pre-launched and Service VM
===========================

The hypervisor is the owner to control pre-launched and Service VM's state
by calling VM APIs directly, and it follows the design of system power
management. Please refer to ACRN power management design for more details.


Post-launched User VMs
======================

DM takes control of post-launched User VMs' state transition after the Service VM
boots, by calling VM APIs through hypercalls.

Service VM user level service such as Life-Cycle-Service and tools such
as Acrnd may work together with DM to launch or stop a User VM. Please
refer to ACRN tool introduction for more details.

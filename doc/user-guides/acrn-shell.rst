.. _acrnshell:

ACRN Shell Commands
###################

The ACRN hypervisor shell supports the following commands:

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Command (and parameters)
     - Description
   * - help
     - Display information about supported hypervisor shell commands
   * - version
     - Display the HV version information
   * - vm_list
     - List all VMs, displaying the VM UUID, ID, name, and state ("Started"=running)
   * - vcpu_list
     - List all vCPUs in all VMs
   * - vcpu_dumpreg <vm_id> <vcpu_id>
     - Dump registers for a specific vCPU
   * - dump_host_mem <hva> <length>
     - Dump the host memory region as specified by the start of the region ``hva`` (in hexadecimal)
       and its length ``length`` (in bytes, decimal number).
   * - dump_guest_mem <vm_id> <gva> <length>
     - Dump a User VM (guest) memory region based on the VM ID (``vm_id``, in decimal),
       the start of the memory region ``gva`` (in hexadecimal) and its length ``length`` (in bytes, decimal number).
   * - vm_console <vm_id>
     - Switch to the VM's console. Use :kbd:`Ctrl` + :kbd:`Space` to return to the ACRN
       shell console
   * - int
     - List interrupt information per CPU
   * - pt
     - Show passthrough device information
   * - vioapic <vm_id>
     - Show virtual IOAPIC (vIOAPIC) information for a specific VM
   * - dump_ioapic
     - Show native IOAPIC information
   * - loglevel <console_loglevel> <mem_loglevel> <npk_loglevel>
     - * If no parameters are given, the command will return the level of
         logging for the console, memory and npk
       * Give (up to) three parameters between ``0`` (none) and ``6`` (verbose)
         to set the loglevel for the console, memory, and npk (in
         that order). If less than three parameters are given, the
         loglevels for the remaining areas will not be changed
   * - cpuid <leaf> [subleaf]
     - Display the CPUID leaf [subleaf], in hexadecimal
   * - rdmsr [-p<pcpu_id>] <msr_index>
     - Read the Model-Specific Register (MSR) at index ``msr_index`` (in
       hexadecimal) for CPU ID ``pcpu_id``
   * - wrmsr [-p<pcpu_id>] <msr_index> <value>
     - Write ``value`` (in hexadecimal) to the Model-Specific Register (MSR) at
       index ``msr_index`` (in hexadecimal) for CPU ID ``pcpu_id``

Command examples
****************

The following sections provide further details and examples for some of these commands.

vm_list
=======

``vm_list`` provides the name of each virtual machine and its corresponding ID and
state.

.. figure:: images/shell_image8.png
   :align: center

   vm_list information

vcpu_list
=========

``vcpu_list`` provides information about virtual CPUs (vCPU), including
the VM ID, PCPU ID, VCPU ID, VCPU ROLE (primary or secondary), and VCPU
STATE (init, paused, running, zombie or unknown).

.. figure:: images/shell_image7.png
   :align: center

   vcpu_list information

vcpu_dumpreg
============

``vcpu_dumpreg vmid cpuid`` provides vCPU related information such as
registers values, etc.

In the following example, we dump vCPU0 RIP register value and get into
the Service VM to search for the currently running function, using these
commands::

   cat /proc/kallsyms | grep RIP_value

As you can see, vCPU0 is running in
function ``acpi_idle_do_entry``.

.. figure:: images/shell_image10.png
   :align: center

   vcpu_dumpreg information

.. figure:: images/shell_image9.png
   :align: center

   system map information

dump_host_mem
=============

``dump_host_mem hva length`` provides the specified memory target data such as
the physical CPU (pCPU) number, etc.

In this example, we know the pCPU active bitmap and physical CPU number
physical memory address through
``build/hypervisor/acrn.map``. (Note that the path for
``acrn.map`` depends on how we build the hypervisor.)

Then we can dump the memory address of the pCPU active bitmap and CPU
number, we will know that pCPU active bitmap is 0x000000000000000f and
pCPU number is 0x0000000000000004.

.. figure:: images/shell_image12.png
   :align: center

   dumpmem information

.. figure:: images/shell_image11.png
   :align: center

   acrn map information

dump_guest_mem
==============

The ``dump_guest_mem`` command can dump guest memory according to the given
VM ID and guest virtual address (``gva``).

In this example, we know the starting address of kernel text segment
in guest console or through the ``system.map`` (Note that the path for
``system.map`` depends on how we build the kernel)

.. figure:: images/shell_image19.png
   :align: center

   guest virtual address

.. figure:: images/shell_image20.png
   :align: center

   guest memory information

vm_console
===========

The ``vm_console`` command switches the ACRN's console to become the VM's console.
Press :kbd:`Ctrl` + :kbd:`Space` to return to the ACRN shell console.

vioapic
=======

``vioapic <vm_id>`` shows the virtual IOAPIC information for a specific
VM. In the following figure, we show the virtual IOPIC information for
VM1:

.. figure:: images/shell_image6.png
   :align: center

   vioapic information

dump_ioapic
===========

``dump_ioapic`` provides IOAPIC information and we can get IRQ number,
IRQ vector number, etc.

.. figure:: images/shell_image14.png
   :align: center

   dump_ioapic information

pt
==

``pt`` provides passthrough detailed information, such as the virtual
machine number, interrupt type, interrupt request, interrupt vector,
trigger mode, etc.

.. figure:: images/shell_image13.png
   :align: center

   pt information

int
===

``int`` provides interrupt information on all CPUs and their corresponding
interrupt vector.

.. figure:: images/shell_image17.png
   :align: center

   int information

cpuid
=====

``cpuid <leaf> [subleaf]`` provides the CPUID leaf [subleaf] in
hexadecimal.

.. figure:: images/shell_image15.png
   :align: center

   cpuid information

rdmsr
=====

We can read model specific register (MSR) to get register
values through ``rdmsr [-p<pcpu_id>] <msr_index>``.

In the following example, we can get IA32_APIC_BASE value of pCPU 0 through
the command::

   rdmsr -p0 1b

and see that 1B (Hexadecimal) is the IA32_APIC_BASE MSR address.

.. figure:: images/shell_image16.png
   :align: center

   IA32_APIC_BASE register information

.. figure:: images/shell_image18.png
   :align: center

   rdmsr information

wrmsr
=====

We can write model specific register (MSR) to set register
values through ``wrmsr [-p<pcpu_id>] <msr_index> <value>``.

In the following example, we can set IA32_APIC_BASE value of pCPU 1 through
the command::

   wrmsr -p1 1b 0xfee00c00

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
     - Displays information about supported hypervisor shell commands
   * - vm_list
     - Lists all VMs, displaying VM Name, VM ID, and VM State (ON=running)
   * - vcpu_list
     - Lists all VCPUs in all VMs
   * - vcpu_dumpreg <vm_id, vcpu_id>
     - Dumps registers for a specific VCPU
   * - dumpmem <hva, length>
     - Dumps host memory, starting a given address, and for
       a given length (in bytes)
   * - sos_console
     - Switches to the SOS's console
   * - int
     - Lists interrupt information per CPU
   * - pt
     - Shows pass-through device information
   * - reboot
     - Triggers a system reboot (immediately)
   * - dump_ioapic
     - Shows native ioapic information
   * - vmexit
     - Shows vmexit profiling
   * - logdump <pcpu_id>
     - Dumps the log buffer for the physical CPU
   * - loglevel [console_loglevel] [mem_loglevel]
     - Get (when no parameters are given)  or Set loglevel [0 (none) - 6 (verbose)] for the console and optionally
       for memory
   * - cpuid <leaf> [subleaf]
     - Displays the CPUID leaf [subleaf], in hexadecimal

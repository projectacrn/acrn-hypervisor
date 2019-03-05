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
   * - vm_list
     - List all VMs, displaying the VM name, ID, and state ("Started"=running)
   * - vcpu_list
     - List all vCPUs in all VMs
   * - vcpu_dumpreg <vm_id> <vcpu_id>
     - Dump registers for a specific vCPU
   * - dumpmem <hva> <length>
     - Dump host memory, starting at a given address, and for a given length
       (in bytes)
   * - sos_console
     - Switch to the SOS's console. Use [Ctrl+Spacebar] to return to the ACRN
       shell console
   * - int
     - List interrupt information per CPU
   * - pt
     - Show pass-through device information
   * - vioapic <vm_id>
     - Show virtual IOAPIC (vIOAPIC) information for a specific VM
   * - dump_ioapic
     - Show native IOAPIC information
   * - loglevel <console_loglevel> <mem_loglevel> <npk_logevel>
     - * If no parameters are given, the command will return the level of
         logging for respectively the console, memory and npk
       * Give (up to) three parameters between ``0`` (none) and ``6`` (verbose)
         to set the loglevel for the console, memory, and npk (in
         that order). If less than three parameters are given, the
         loglevels for the remaining areas will not be changed
   * - cpuid <leaf> [subleaf]
     - Display the CPUID leaf [subleaf], in hexadecimal
   * - reboot
     - Trigger a system reboot (immediately)

 .. _asa:

Security Advisory
#################

Addressed in ACRN v2.1
************************

We recommend that all developers upgrade to this v2.1 release (or later), which
addresses the following security issue that was discovered in previous releases:

------

- Missing access control restrictions in the Hypervisor component
   A malicious entity with root access in the Service VM
   userspace could abuse the PCIe assign/de-assign Hypercalls via crafted
   ioctls and payloads.  This attack can result in a corrupt state and Denial
   of Service (DoS) for previously assigned PCIe devices to the Service VM
   at runtime.

   **Affected Release:** v2.0 and v1.6.1.

Addressed in ACRN v1.6.1
************************

We recommend that all developers upgrade to this v1.6.1 release (or later), which
addresses the following security issue that was discovered in previous releases:

------

- Service VM kernel Crashes When Fuzzing HC_ASSIGN_PCIDEV and HC_DEASSIGN_PCIDEV
   NULL pointer dereference due to invalid address of PCI device to be assigned or
   de-assigned may result in kernel crash. The return value of 'pci_find_bus()' shall
   be validated before using in 'update_assigned_vf_state()'.

   **Affected Release:** v1.6.


Addressed in ACRN v1.6
**********************

We recommend that all developers upgrade to this v1.6 release (or later), which
addresses the following security issues that were discovered in previous releases:

------

- Hypervisor Crashes When Fuzzing HC_DESTROY_VM
   The input 'vdev->pdev' should be validated properly when handling
   HC_SET_PTDEV_INTR_INFO to ensure that the physical device is linked to
   'vdev'; otherwise, the hypervisor crashes when fuzzing the
   hypercall HC_DESTROY_VM with crafted input.

   **Affected Release:** v1.5 and earlier.

- Hypervisor Crashes When Fuzzing HC_VM_WRITE_PROTECT_PAGE
   The input GPA is not validated when handling this hypercall; an "Invalid
   GPA" that is not in the scope of the target VM's EPT address space results
   in the hypervisor crashing when handling this hypercall.

   **Affected Release:** v1.4 and earlier.

- Hypervisor Crashes When Fuzzing HC_NOTIFY_REQUEST_FINISH
   The input is not validated properly when handing this hypercall;
   'vcpu_id' should be less than 'vm->hw.created_vcpus' instead of
   'MAX_VCPUS_PER_VM'. When the software fails to validate input properly,
   the hypervisor crashes when handling crafted inputs.

   **Affected Release:** v1.4 and earlier.


Addressed in ACRN v1.4
**********************

We recommend that all developers upgrade to this v1.4 release (or later), which
addresses the following security issues that were discovered in previous releases:

------

- Mitigation for Machine Check Error on Page Size Change
   Improper invalidation for page table updates by a virtual guest operating
   system for multiple Intel(R) Processors may allow an authenticated user
   to potentially enable denial of service of the host system via local
   access. A malicious guest kernel could trigger this issue, CVE-2018-12207.

   **Affected Release:** v1.3 and earlier.

- AP Trampoline Is Accessible to the Service VM
   This vulnerability is triggered when validating the memory isolation
   between the VM and the hypervisor. The AP Trampoline code exists in the
   LOW_RAM region of the hypervisor but is potentially accessible to the
   Service VM. This could be used by an attacker to mount DoS attacks on the
   hypervisor if the Service VM is compromised.

   **Affected Release:** v1.3 and earlier.

- Improper Usage Of the ``LIST_FOREACH()`` Macro
   Testing discovered that the MACRO ``LIST_FOREACH()`` was incorrectly used
   in some cases which could induce a "wild pointer" and cause the ACRN
   Device Model to crash. Attackers can potentially use this issue to cause
   denial of service (DoS) attacks.

   **Affected Release:** v1.3 and earlier.

- Hypervisor Crashes When Fuzzing HC_SET_CALLBACK_VECTOR
   This vulnerability was reported by the Fuzzing tool for the debug version
   of ACRN. When the software fails to validate input properly, an attacker
   is able to craft the input in a form that is not expected by the rest of
   the application. This can lead to parts of the system receiving
   unintended inputs, which may result in an altered control flow, arbitrary
   control of a resource, or arbitrary code execution.

   **Affected Release:** v1.3 and earlier.

- FILE Pointer Is Not Closed After Using
   This vulnerability was reported by the Fuzzing tool. Leaving the file
   unclosed will cause a leaking file descriptor and may cause unexpected
   errors in the Device Model program.

   **Affected Release:** v1.3 and earlier.

- Descriptor of Directory Stream Is Referenced After Release
   This vulnerability was reported by the Fuzzing tool. A successful call to
   ``closedir(DIR *dirp)`` also closes the underlying file descriptor
   associated with ``dirp``. Access to the released descriptor may point to
   some arbitrary memory location or cause undefined behavior.

   **Affected Release:** v1.3 and earlier.

- Mutex Is Potentially Kept in a Locked State Forever
   This vulnerability was reported by the Fuzzing tool. Here,
   pthread_mutex_lock/unlock pairing was not always done. Leaving a mutex in
   a locked state forever can cause program deadlock, depending on the usage
   scenario.

   **Affected Release:** v1.3 and earlier.

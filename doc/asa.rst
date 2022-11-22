.. _asa:

Security Advisory
#################

Addressed in ACRN v3.0.2
************************
We recommend that all developers using v3.0.1 or earlier upgrade to this v3.0.2
release (or later), which addresses the following security issue discovered in
previous releases. For v3.1 users, these issues are addressed in the v3.2
release:

-----

- Board_inspector: use executables found under system paths
    Using partial executable paths in the board inspector may cause unintended
    results when another executable has the same name and is also detectable in
    the search paths.

    Introduce a wrapper module (`external_tools`) which locates executables
    only under system paths such as /usr/bin and /usr/sbin and converts partial
    executable paths to absolute ones before executing them via the subprocess
    module. All invocations to `subprocess.run` or `subprocess.Popen`
    throughout the board inspector are replaced with `external_tools.run`, with
    the only exception being the invocation to the legacy board parser which
    already uses an absolute path to the current Python interpreter.

    **Affected Release:** v3.1, v3.0.1 and earlier

- Add tarfile member sanitization to extractall()
    A directory traversal vulnerability in the Python tarfile module extractall() functions
    could allow user-assisted remote attackers to overwrite arbitrary files via
    a ``..`` (dot dot) sequence in filenames in a tar archive, related to CVE-2001-1267.
    (Addresses security issue tracked by CVE-2007-4559)

    **Affected Release:** v3.1, v3.0.1 and earlier

- PMU (Performance Monitoring Unit) is passed through to an RTVM only for debug mode
    Enabling Pass-through PMU counters to RTVM can cause workload interference
    in a release build, so enable PMU passthrough only when building ACRN in
    debug mode.

    **Affected Release:** v3.1, v3.0.1 and earlier

Addressed in ACRN v3.0.1
************************
We recommend that all developers upgrade to this v3.0.1 release (or later), which
addresses the following security issue discovered in previous releases:

-----

-  Disable RRSBA on platforms using retpoline
    For platforms that supports RRSBA (Restricted Return Stack Buffer
    Alternate), using retpoline may not be sufficient to guard against branch
    history injection or intra-mode branch target injection. RRSBA must
    be disabled to prevent CPUs from using alternate predictors for RETs.
    (Addresses security issue tracked by CVE-2022-29901 and CVE-2022-28693.)

    **Affected Release:** v3.0 and earlier

Addressed in ACRN v2.7
************************
We recommend that all developers upgrade to this v2.7 release (or later), which
addresses the following security issue discovered in previous releases:

-----

-  Heap-use-after-free happens in ``MEVENT mevent_handle``
    The file descriptor of ``mevent`` could be closed in another thread while being
    monitored by ``epoll_wait``. This causes a heap-use-after-free error in
    the ``mevent_handle()`` function.

    **Affected Release:** v2.6 and earlier

Addressed in ACRN v2.6
************************

We recommend that all developers upgrade to this v2.6 release (or later), which
addresses the following security issue discovered in previous releases:

-----

-  Memory leakage vulnerability in ``devicemodel/hw/pci/xhci.c``
    De-initializing of emulated USB devices results in a memory leakage issue
    as some resources allocated for transfer are not properly released.

    **Affected Release:** v2.5 and earlier.


Addressed in ACRN v2.5
************************

We recommend that all developers upgrade to this v2.5 release (or later), which
addresses the following security issues that were discovered in previous releases:

-----

-  NULL Pointer Dereference in ``devicemodel/hw/pci/virtio/virtio_net.c``
    ``virtio_net_ping_rxq()`` function tries to set ``vq->used->flags`` without
    validating pointer ``vq->used``, which may be NULL and cause a NULL pointer dereference.

    **Affected Release:** v2.4 and earlier.

-  NULL Pointer Dereference in ``hw/pci/virtio/virtio.c``
    ``vq_endchains`` function tries to read ``vq->used->idx`` without
    validating pointer ``vq->used``, which may be NULL and cause a NULL pointer dereference.

    **Affected Release:** v2.4 and earlier.

-  NULL Pointer Dereference in ``devicemodel/hw/pci/xhci.c``
    The ``trb`` pointer in ``pci_xhci_complete_commands`` function may be from user space and may be NULL.
    Accessing it without validating may cause a NULL pointer dereference.

    **Affected Release:** v2.4 and earlier.

-  Buffer overflow in ``hypervisor/arch/x86/vtd.c``
    Malicious input ``index`` for function ``dmar_free_irte`` may trigger buffer
    overflow on array ``irte_alloc_bitmap[]``.

    **Affected Release:** v2.4 and earlier.

-  Page Fault in ``devicemodel/core/mem.c``
    ``unregister_mem_int()`` function frees any entry when it is valid, which is not expected.
    (only entries to be removed from RB tree can be freed). This will cause a page fault
    when next RB tree iteration happens.

    **Affected Release:** v2.4 and earlier

-  Heap-use-after-free happens in VIRTIO timer_handler
    With virtio polling mode enabled, a timer is running in the virtio
    backend service. The timer will also be triggered if its frontend
    driver didn't do the device reset on shutdown. A freed virtio device
    could be accessed in the polling timer handler.

    **Affected Release:** v2.4 and earlier

Addressed in ACRN v2.3
************************

We recommend that all developers upgrade to this v2.3 release (or later), which
addresses the following security issue that was discovered in previous releases:

-----

- NULL Pointer Dereference in ``devicemodel\hw\pci\virtio\virtio_mei.c``
   ``vmei_proc_tx()`` function tries to find the ``iov_base`` by calling
   function ``paddr_guest2host()``, which may return NULL (the ``vd``
   struct control by the User VM OS).  There is a use of ``iov_base``
   afterward that can cause a NULL pointer dereference (CVE-2020-28346).

   **Affected Release:** v2.2 and earlier.

Addressed in ACRN v2.1
************************

We recommend that all developers upgrade to this v2.1 release (or later), which
addresses the following security issue that was discovered in previous releases:

-----

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

-----

- Service VM kernel Crashes When Fuzzing HC_ASSIGN_PCIDEV and HC_DEASSIGN_PCIDEV
   NULL pointer dereference due to invalid address of PCI device to be assigned or
   de-assigned may result in kernel crash. The return value of 'pci_find_bus()' shall
   be validated before using in 'update_assigned_vf_state()'.

   **Affected Release:** v1.6.


Addressed in ACRN v1.6
**********************

We recommend that all developers upgrade to this v1.6 release (or later), which
addresses the following security issues that were discovered in previous releases:

-----

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

-----

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

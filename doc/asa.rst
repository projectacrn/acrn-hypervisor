.. _asa:

Advisory
********

We recommend that all developers upgrade to this v1.4 release, which addresses the following security
issues that were discovered in previous releases:

Mitigation for Machine Check Error on Page Size Change
   Improper invalidation for page table updates by a virtual guest operating system for multiple Intel(R) Processors may allow an authenticated user to potentially enable denial of service of the host system via local access. Malicious guest kernel could trigger this issue, CVE-2018-12207.

   | **Affected Release:** v1.3 and earlier.
   | Upgrade to ACRN release v1.4.

AP Trampoline Is Accessible to the Service VM
   This vulnerability is triggered when validating the memory isolation between the VM and hypervisor. The AP Trampoline code exists in the LOW_RAM region in the hypervisor but is
   potentially accessible to the Service VM. This could be used by an attacker to mount DoS
   attacks on the hypervisor if the Service VM is compromised.

   | **Affected Release:** v1.3 and earlier.
   | Upgrade to ACRN release v1.4.

Improper Usage Of the ``LIST_FOREACH()`` Macro
   Testing discovered that the MACRO ``LIST_FOREACH()`` was incorrectly used in some cases
   which could induce a "wild pointer" and cause the ACRN Device Model to crash. Attackers
   can potentially use this issue to cause denial of service (DoS) attacks.

   | **Affected Release:** v1.3 and earlier.
   | Upgrade to ACRN release v1.4.

Hypervisor Crashed When Fuzzing HC_SET_CALLBACK_VECTOR
   This vulnerability was reported by the Fuzzing tool for the debug version of ACRN. When the software fails
   to validate input properly, an attacker is able to craft the input in a form that is
   not expected by the rest of the application. This can lead to parts of the system
   receiving unintended inputs, which may result in an altered control flow, arbitrary control
   of a resource, or arbitrary code execution.

   | **Affected Release:** v1.3 and earlier.
   | Upgrade to ACRN release v1.4.

FILE Pointer Is Not Closed After Using
   This vulnerability was reported by the Fuzzing tool. Leaving the file unclosed will cause a
   leaking file descriptor and may cause unexpected errors in the Device Model program.

   | **Affected Release:** v1.3 and earlier.
   | Upgrade to ACRN release v1.4.

Descriptor of Directory Stream Is Referenced After Release
   This vulnerability was reported by the Fuzzing tool. A successful call to ``closedir(DIR *dirp)``
   also closes the underlying file descriptor associated with ``dirp``. Access to the released
   descriptor may point to some arbitrary memory location or cause undefined behavior.

   | **Affected Release:** v1.3 and earlier.
   | Upgrade to ACRN release v1.4.

Mutex Is Potentially Kept in a Locked State Forever
   This vulnerability was reported by the Fuzzing tool. Here, pthread_mutex_lock/unlock pairing was not
   always done. Leaving a mutex in a locked state forever can cause program deadlock,
   depending on the usage scenario.

   | **Affected Release:** v1.3 and earlier.
   | Upgrade to ACRN release v1.4.

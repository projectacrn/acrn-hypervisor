.. _asa:

Advisory 
********

We recommend all developers upgrade to this v1.4 release, which addresses these security
issues discovered in earlier releases:

AP Trampoline Is Accessible to Service VM
   This vulnerability is triggered when validating the memory isolation between
   VM and hypervisor. AP Trampoline code exists in LOW_RAM region in hypervisor but is
   potentially accessible to service VM. This could be used by an attacker to mount DoS
   attacks on the hypervisor if service VM is compromised.

   | **Affected Release:** v1.3 and earlier.
   | It’s recommended to upgrade ACRN to release v1.4

Improper Usage Of ``LIST_FOREACH()`` macro
   Testing discovered that the MACRO ``LIST_FOREACH()`` was incorrectly used for some cases
   which may induce a "wild pointer" and cause ACRN Device Model crash. An attacker
   could use this issue to cause a denial of service (DoS). 

   | **Affected Release:** v1.3 and earlier.
   | It’s recommended to upgrade ACRN to release v1.4

Hypervisor Crashed When Fuzzing HC_SET_CALLBACK_VECTOR
   This vulnerability was reported by Fuzzing tool for debug version of ACRN. When software fails
   to validate input properly, an attacker is able to craft the input in a form that is
   not expected by the rest of the application. This can lead to parts of the system
   receiving unintended input, which may result in altered control flow, arbitrary control
   of a resource, or arbitrary code execution.

   | **Affected Release:** v1.3 and earlier.
   | It’s recommended to upgrade ACRN to release v1.4

FILE Pointer Is Not Closed After Using
   This vulnerability was reported by Fuzzing tool. Leaving the file unclosed will cause
   leaking file descriptor and may cause unexpected errors in Device Model program.

   | **Affected Release:** v1.3 and earlier.
   | It’s recommended to upgrade ACRN to release v1.4

Descriptor of Directory Stream Is Referenced After Release
   This vulnerability was reported by Fuzzing tool. A successful call to ``closedir(DIR *dirp)``
   also closes the underlying file descriptor associated with ``dirp``. Access to the released
   descriptor may point to some arbitrary memory location or cause undefined behavior.

   | **Affected Release:** v1.3 and earlier.
   | It’s recommended to upgrade ACRN to release v1.4

Mutex Is Potentially Kept in Locked State Forever
   This vulnerability was reported by Fuzzing tool. pthread_mutex_lock/unlock pairing was not
   always done. Leaving a mutex in a locked state forever can cause program deadlock,
   depending on the usage scenario.

   | **Affected Release:** v1.3 and earlier.
   | It’s recommended to upgrade ACRN to release v1.4

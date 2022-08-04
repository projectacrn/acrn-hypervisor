.. _release_notes_3.0.1:

ACRN v3.0.1 (Jul 2022)
######################

We are pleased to announce the release of the Project ACRN hypervisor
version 3.0.1 with hot fixes to the v3.0 release.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline
embedded development through an open-source platform. See the
:ref:`introduction` introduction for more information.

All project ACRN source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, tools, and
documentation. You can download this source code either as a zip or
tar.gz file (see the `ACRN v3.0.1 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v3.0.1>`_) or
use Git ``clone`` and ``checkout`` commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v3.0.1

The project's online technical documentation is also tagged to
correspond with a specific release: generated v3.0 documents can be
found at https://projectacrn.github.io/3.0/.  Documentation for the
latest development branch is found at https://projectacrn.github.io/latest/.

ACRN v3.0.1 requires Ubuntu 20.04 (as does v3.0).  Follow the instructions in the
:ref:`gsg` to get started with ACRN.


What's New in v3.0.1
********************

Mitigation for Return Stack Buffer Underflow security vulnerability
  When running ACRN on Alder Lake platforms that support RRSBA (Restricted Return Stack Buffer
  Alternate), using retpoline may not be sufficient to guard against branch
  history injection or intra-mode branch target injection. RRSBA must
  be disabled for Alder Lake platforms to prevent CPUs from using alternate predictors for RETs.
  (Addresses security issue tracked by CVE-2022-29901 and CVE-2022-28693.)

ACRN shell commands added for real-time performance profiling
  ACRN shell commands were added to sample vmexit data per virtual CPU to
  facilitate real-time performance profiling:

  * ``vmexit enable | disable``: enabled by default
  * ``vmexit clear``: clears current vmexit buffer
  * ``vmexit [vm_id]``: outputs vmexit reason code and latency count information per vCPU
    for a VM ID (or for all VM IDs if none is specified).

See :ref:`release_notes_3.0` for additional release information.

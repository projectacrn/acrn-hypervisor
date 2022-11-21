.. _release_notes_3.0.2:

ACRN v3.0.2 (Nov 2022)
######################

We are pleased to announce the release of the Project ACRN hypervisor
version 3.0.2 with hot fixes to the v3.0 release.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline
embedded development through an open-source platform. See the
:ref:`introduction` introduction for more information.

All project ACRN source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, tools, and
documentation. You can download this source code either as a zip or
tar.gz file (see the `ACRN v3.0.2 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v3.0.2>`_) or
use Git ``clone`` and ``checkout`` commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v3.0.2

The project's online technical documentation is also tagged to
correspond with a specific release: generated v3.0 documents can be
found at https://projectacrn.github.io/3.0/.  Documentation for the
latest development branch is found at https://projectacrn.github.io/latest/.

ACRN v3.0.2 requires Ubuntu 20.04 (as does v3.0).  Follow the instructions in the
:ref:`gsg` to get started with ACRN.


What's New in v3.0.2
********************

Passthrough PMU (performance monitor unit) to user VM only in debug builds
  ACRN v2.6 introduced PMU passthrough to RT VMs that have LAPIC passthrough
  enabled. This is useful for performance profiling at development time but can
  cause workload interference in a production build. PMU passthrough is only
  enabled now for a hypervisor debug mode build.

Added tarfile member sanitization to Python tarfile package extractall() calls
  A vulnerability in the ACRN Configurator is patched, where files extracted
  from a maliciously crafted tarball could  be written to somewhere outside the
  target directory and cause unsafe behavior.

Run executables with absolute paths in board inspector
  Using partial executable paths in the board inspector may cause unintended
  results when another executable has the same name and is found via PATH
  settings. The board inspector now uses absolute paths to executable.



See :ref:`release_notes_3.0` and :ref:`release_notes_3.0.1` for additional release information.

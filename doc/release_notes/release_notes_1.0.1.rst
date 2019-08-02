.. _release_notes_1.0.1:

ACRN v1.0.1 (July 2019)
#######################

We are pleased to announce the release of ACRN version 1.0.1. This is a
maintenance release based off the v1.0 branch that primarily fixes some
potential security and stability issues discovered after the v1.0
release.

ACRN is a flexible, lightweight reference hypervisor that's built with
real-time and safety-criticality in mind and is optimized to streamline
embedded development through an open source platform.  Check out
:ref:`introduction` for more information. All project ACRN source code
is maintained in the https://github.com/projectacrn/acrn-hypervisor
repository and includes folders for the ACRN hypervisor, the ACRN device
model, tools, and documentation. You can either download this source
code as a zip or tar.gz file (see the `ACRN v1.0.1 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v1.0.1>`_)
or use the following Git clone and checkout commands::

   $ git clone https://github.com/projectacrn/acrn-hypervisor
   $ cd acrn-hypervisor
   $ git checkout v1.0.1

There were no documentation changes in this update, so you can still
refer to the v1.0-specific documentation found at
https://projectacrn.github.io/1.0/.

Change Log in version 1.0.1 since version 1.0
*********************************************

Primary changes are to fix several security and stability issues found
on the v1.0 branch, as listed here:

.. csv-table::
   :header: "GIT issue ID", "Commit ID", "Description"
   :widths: 15,15,70

   :acrn-issue:`3245` , :acrn-commit:`5ced5fe7`, dm: use strncpy to replace strcpy
   :acrn-issue:`3276` , :acrn-commit:`5530fc8f`, efi-stub: update string operation in efi-stub
                      , :acrn-commit:`b65489c2`, dm: use strnlen to replace strlen
   :acrn-issue:`3277` , :acrn-commit:`0c0371fc`, dm: fix stability issue in mem.c and xhci.c
   :acrn-issue:`3427` , :acrn-commit:`749556ef`, hv: fix symbols not stripped from release binaries
   :acrn-issue:`3395` , :acrn-commit:`bc90db46`, dm: fix stability issue in block_if.c
   :acrn-issue:`3396` , :acrn-commit:`2e7171d6`, dm: fix variable argument list read without ending with va_end
                      , :acrn-commit:`1394758d`, tools: fix variable argument list read without ending with va_end
   :acrn-issue:`3397` , :acrn-commit:`d6f72885`, hv: fix stability issue in hypervisor

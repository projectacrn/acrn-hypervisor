.. _release_notes_1.0.2:

ACRN v1.0.2 (Nov 2019)
######################

We are pleased to announce the release of ACRN version 1.0.2. This is a
maintenance release based on the v1.0 branch that primarily fixes some
potential security and stability issues discovered after the v1.0
release.

ACRN is a flexible, lightweight reference hypervisor that's built with
real-time and safety-criticality in mind and is optimized to streamline
embedded development through an open source platform.  Check out
:ref:`introduction` for more information. All project ACRN source code
is maintained in the https://github.com/projectacrn/acrn-hypervisor
repository and includes folders for the ACRN hypervisor, the ACRN device
model, tools, and documentation. You can either download this source
code as a zip or tar.gz file (see the `ACRN v1.0.2 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v1.0.2>`_)
or use the following Git clone and checkout commands::

   $ git clone https://github.com/projectacrn/acrn-hypervisor
   $ cd acrn-hypervisor
   $ git checkout v1.0.2

There were no documentation changes in this update, so you can still
refer to the v1.0-specific documentation found at
https://projectacrn.github.io/1.0/.

Change Log in v1.0.2 since v1.0.1
*********************************

Primary changes are to fix several security and stability issues found
on the v1.0 branch, as listed here:

.. csv-table::
   :header: "GIT issue ID", "Commit ID", "Description"
   :widths: 15,15,70

   :acrn-issue:`4120` , :acrn-commit:`a94b2a53`, hv: ept: disable execute right on large pages
   :acrn-issue:`4120` , :acrn-commit:`1a99288d`, hv: vtd: remove global cache invalidation per vm
   :acrn-issue:`4120` , :acrn-commit:`2d55b49e`, hv: ept: flush cache for modified ept entries
   :acrn-issue:`4120` , :acrn-commit:`a6944fe6`, hv: vtd: export iommu_flush_cache
   :acrn-issue:`4091` , :acrn-commit:`30a773f7`, hv:unmap AP trampoline region from service VM's EPT
   :acrn-issue:`4091` , :acrn-commit:`0b6447ad`, hv:refine modify_or_del_pte/pde/pdpte()function
   :acrn-issue:`4093` , :acrn-commit:`b1951490`, acrn-hv: code review fix lib/string.c
   :acrn-issue:`4089` , :acrn-commit:`6730660a`, tools: acrn-crashlog: refine crash complete code
   :acrn-issue:`4088` , :acrn-commit:`aba91a81`, vm-manager: fix improper return value check for "strtol()"
   :acrn-issue:`4087` , :acrn-commit:`995efc1b`, dm: refine the check of return value of snprintf
   :acrn-issue:`4086` , :acrn-commit:`720a77c1`, dm: fix mutex lock issue in tpm_rbc.c
   :acrn-issue:`4085` , :acrn-commit:`b51b8980`, dm: close filepointer before exiting acrn_load_elf()
   :acrn-issue:`4084` , :acrn-commit:`84c3ee21`, dm: modify DIR handler reference postion
   :acrn-issue:`4083` , :acrn-commit:`4baccdce`, dm: reduce potential crash caused by LIST_FOREACH
   :acrn-issue:`4092` , :acrn-commit:`2e054f6c`, hv: fix error debug message in hcall_set_callback_vector
   :acrn-issue:`4003` , :acrn-commit:`6199e653`, dm: validate the input in 'pci_emul_mem_handler()'

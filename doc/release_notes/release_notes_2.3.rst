.. _release_notes_2.3:

ACRN v2.3 (Dec 2020)
####################

We are pleased to announce the release of the Project ACRN
hypervisor version 2.3.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline
embedded development through an open source platform. Check out the
:ref:`introduction` introduction for more information.  All project ACRN
source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, tools, and
documentation. You can either download this source code as a zip or
tar.gz file (see the `ACRN v2.3 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v2.3>`_) or
use Git clone and checkout commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v2.3

The project's online technical documentation is also tagged to
correspond with a specific release: generated v2.3 documents can be
found at https://projectacrn.github.io/2.3/.  Documentation for the
latest under-development branch is found at
https://projectacrn.github.io/latest/.

ACRN v2.3 requires Ubuntu 18.04.  Follow the instructions in the
:ref:`rt_industry_ubuntu_setup` to get started with ACRN.


What's New in v2.3
******************

Enhanced GPU passthru (GVT-d)
  GPU passthru (GVT-d) to Windows as a guest is now enabled for 11th Gen
  Intel® Core™ processors (codenamed Tiger Lake-UP3).

Shared memory based inter-VM communication (ivshmem) is extended
  ivshmem now supports interrupts (See :ref:`ivshmem-hld`).

Enhanced vUART support
  Added console support using legacy vUART (0x3F8-like) and
  added PCI vUART (up to 8) for VM-to-VM communication.

End-to-end secure boot improvement
  OVMF can be loaded now as two blobs, one for code and the other for data.
  The code blob can be verified by the Service VM's ``dm-verify`` as
  a step in the end-to-end secure boot.

Enhanced system shutdown
  The pre-launched VM may now initiate a system shutdown or reset.

Removed deprivileged boot mode support
  ACRN has supported deprivileged boot mode to ease the integration of
  Linux distributions such as Clear Linux. Unfortunately, deprivileged boot
  mode limits ACRN's scalability and is unsuitable for ACRN's hybrid
  hypervisor mode. In ACRN v2.2, deprivileged boot mode was no longer the default
  and completely removed in ACRN v2.3. We're focusing instead
  on using the simpler and more scalable multiboot2 boot (via Grub).
  Multiboot2 is not supported in
  Clear Linux so we have chosen Ubuntu (and Yocto Project) as the
  preferred Service VM OSs moving forward.

Document updates
****************

New and updated reference documents are available, including:

.. rst-class:: rst-columns2

* :ref:`GVT-g-porting`
* :ref:`vbsk-overhead`
* :ref:`asm_coding_guidelines`
* :ref:`c_coding_guidelines`
* :ref:`contribute_guidelines`
* :ref:`doc_guidelines`
* :ref:`hld-devicemodel`
* :ref:`hld-overview`
* :ref:`hld-power-management`
* :ref:`hld-security`
* :ref:`hld-trace-log`
* :ref:`hld-virtio-devices`
* :ref:`ivshmem-hld`
* :ref:`l1tf`
* :ref:`modularity`
* :ref:`sw_design_guidelines`
* :ref:`rt_industry_ubuntu_setup`
* :ref:`introduction`
* :ref:`release_notes_2.2`
* :ref:`acrn_configuration_tool`
* :ref:`acrn_on_qemu`
* :ref:`acrn-debug`
* :ref:`acrn_doc`
* :ref:`enable_ivshmem`
* :ref:`enable-s5`
* :ref:`rdt_configuration`
* :ref:`rt_performance_tuning`
* :ref:`rt_perf_tips_rtvm`
* :ref:`run-kata-containers`
* :ref:`running_deb_as_user_vm`
* :ref:`running_ubun_as_user_vm`
* :ref:`setup_openstack_libvirt`
* :ref:`sgx_virt`
* :ref:`sriov_virtualization`
* :ref:`using_grub`
* :ref:`using_hybrid_mode_on_nuc`
* :ref:`using_partition_mode_on_nuc`
* :ref:`using_windows_as_uos`
* :ref:`using_zephyr_as_uos`
* :ref:`vuart_config`
* :ref:`how-to-enable-secure-boot-for-windows`
* :ref:`acrn-dm_parameters`

Because we're dropped deprivileged boot mode support,
we're also switching our Service VM of choice away from Clear
Linux and have removed
Clear Linux-specific tutorials.  Deleted documents are still
available in the `version-specific v2.1 documentation
<https://projectacrn.github.io/v2.1/>`_.


Fixed Issues Details
********************

.. comment list items look like this (not indented)
   - :acrn-issue:`5008` -  Slowdown in UOS (Zephyr)

Known Issues
************

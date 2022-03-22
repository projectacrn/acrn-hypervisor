.. _release_notes_2.2:

ACRN v2.2 (Sep 2020)
####################

We are pleased to announce the release of the Project ACRN
hypervisor version 2.2.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline
embedded development through an open source platform. Check out the
:ref:`introduction` introduction for more information.  All project ACRN
source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, tools, and
documentation. You can either download this source code as a zip or
tar.gz file (see the `ACRN v2.2 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v2.2>`_) or
use Git clone and checkout commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v2.2

The project's online technical documentation is also tagged to
correspond with a specific release: generated v2.2 documents can be
found at https://projectacrn.github.io/2.2/.  Documentation for the
latest under-development branch is found at
https://projectacrn.github.io/latest/.

ACRN v2.2 requires Ubuntu 18.04.  Follow the instructions in the
:ref:`rt_industry_ubuntu_setup` to get started with ACRN.


What's New in v2.2
******************

Elkhart Lake and Tiger Lake processor support.
  At `Intel Industrial iSummit 2020
  <https://newsroom.intel.com/press-kits/intel-industrial-summit-2020>`_,
  Intel announced the latest additions to their
  enhanced-for-IoT Edge portfolio: the Intel |reg| Atom |reg| x6000E Series, Intel |reg|
  Pentium |reg| and Intel |reg| Celeron |reg| N and J Series (all codenamed Elkhart Lake),
  and 11th Gen Intel |reg| Core |trade| processors (codenamed Tiger Lake-UP3). The ACRN
  team is pleased to announce that this ACRN v2.2 release already supports
  these processors.

  * Support for time deterministic applications with new features, e.g.,
    Time Coordinated Computing and Time Sensitive Networking
  * Support for functional safety with new features, e.g., Intel Safety Island

On Elkhart Lake, ACRN can boot using Slim Bootloader
  `Slim Bootloader <https://slimbootloader.github.io/>`_ is an
  alternative bootloader to UEFI firmware.

Shared memory based inter-VM communication (ivshmem) is extended
  ivshmem now supports all kinds of VMs including pre-launched VM, Service VM, and
  other User VMs. (See :ref:`ivshmem-hld`)

**CPU sharing supports pre-launched VM.**

**RTLinux with preempt-RT Linux kernel 5.4 is validated both as a pre-launched and post-launched VM.**

**ACRN hypervisor can emulate MSI-X based on physical MSI with multiple vectors.**

Staged removal of deprivileged boot mode support.
  ACRN has supported deprivileged boot mode to ease the integration of
  Linux distributions such as Clear Linux. Unfortunately, deprivileged boot
  mode limits ACRN's scalability and is unsuitable for ACRN's hybrid
  hypervisor mode. In ACRN v2.2, deprivileged boot mode is no longer the default
  and will be completely removed in ACRN v2.3. We're focusing instead
  on using multiboot2 boot (via Grub). Multiboot2 is not supported in
  Clear Linux though, so we have chosen Ubuntu (and Yocto Project) as the
  preferred Service VM OSs moving forward.

Document Updates
****************

New and updated reference documents are available, including:

.. rst-class:: rst-columns2

* :ref:`develop_acrn`
* :ref:`asm_coding_guidelines`
* :ref:`c_coding_guidelines`
* :ref:`contribute_guidelines`
* :ref:`hv-cpu-virt`
* :ref:`IOC_virtualization_hld`
* :ref:`hv-startup`
* :ref:`hv-vm-management`
* :ref:`ivshmem-hld`
* :ref:`virtio-i2c`
* :ref:`sw_design_guidelines`
* :ref:`faq`
* :ref:`getting-started-building`
* :ref:`introduction`
* :ref:`acrn_configuration_tool`
* :ref:`enable_ivshmem`
* :ref:`setup_openstack_libvirt`
* :ref:`using_grub`
* :ref:`using_partition_mode_on_nuc`
* :ref:`connect_serial_port`
* Using Yocto Project With ACRN
* :ref:`acrn-dm_parameters`
* :ref:`hv-parameters`
* :ref:`acrnctl`

Because we're dropping deprivileged boot mode support in the next v2.3
release, we're also switching our Service VM of choice away from Clear
Linux. We've begun this transition in the v2.2 documentation and removed
some Clear Linux-specific tutorials.  Deleted documents are still
available in the `version-specific v2.1 documentation
<https://projectacrn.github.io/v2.1/>`_.


Fixed Issues Details
********************
- :acrn-issue:`5008` -  Slowdown in UOS (Zephyr)
- :acrn-issue:`5033` -  SOS decode instruction failed in hybrid mode
- :acrn-issue:`5038` -  [WHL][Yocto] SOS occasionally hangs/crashes with a kernel panic
- :acrn-issue:`5048` -  iTCO_wdt issue: can't request region for resource
- :acrn-issue:`5102` -  Can't access shared memory base address in ivshmem
- :acrn-issue:`5118` -  GPT ERROR when write preempt img to SATA on NUC7i5BNB
- :acrn-issue:`5148` -  dm: support to provide ACPI SSDT for UOS
- :acrn-issue:`5157` -  [build from source] during build HV with XML, "TARGET_DIR=xxx" does not work
- :acrn-issue:`5165` -  [WHL][Yocto][YaaG] No UI display when launch Yaag gvt-g with acrn kernel
- :acrn-issue:`5215` -  [UPsquared N3350 board] Solution to Bootloader issue
- :acrn-issue:`5233` -  Boot ACRN failed on Dell-OptiPlex 5040 with Intel i5-6500T
- :acrn-issue:`5238` -  acrn-config: add hybrid_rt scenario XML config for ehl-crb-b
- :acrn-issue:`5240` -  passthrough DHRD-ignored device
- :acrn-issue:`5242` -  acrn-config: add pse-gpio to vmsix_on_msi devices list
- :acrn-issue:`4691` -  hv: add vgpio device model support
- :acrn-issue:`5245` -  hv: add INTx mapping for pre-launched VMs
- :acrn-issue:`5426` -  hv: add vgpio device model support
- :acrn-issue:`5257` -  hv: support PIO access to platform hidden devices
- :acrn-issue:`5278` -  [EHL][acrn-configuration-tool]: create a new hybrid_rt based scenario for P2SB MMIO pass-thru use case
- :acrn-issue:`5304` -  Cannot cross-compile - Build process assumes build system always hosts the ACRN hypervisor

Known Issues
************
- :acrn-issue:`5150` - [REG][WHL][[Yocto][Passthru] Launch RTVM fails with USB passthru
- :acrn-issue:`5151` - [WHL][VxWorks] Launch VxWorks fails due to no suitable video mode found
- :acrn-issue:`5154` - [TGL][Yocto][PM] 148213_PM_SystemS5 with life_mngr fail
- :acrn-issue:`5368` - [TGL][Yocto][Passthru] Audio does not work on TGL
- :acrn-issue:`5369` - [TGL][qemu] Cannot launch qemu on TGL
- :acrn-issue:`5370` - [TGL][RTVM][PTCM] Launch RTVM failed with mem size smaller than 2G and PTCM enabled
- :acrn-issue:`5371` - [TGL][Industry][Xenomai]Xenomai post launch fail

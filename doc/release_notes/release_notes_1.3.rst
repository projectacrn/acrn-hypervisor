.. _release_notes_1.3:

ACRN v1.3 (Sep 2019)
####################

We are pleased to announce the release of ACRN version 1.3.

ACRN is a flexible, lightweight reference hypervisor, built with
real-time and safety-criticality in mind, optimized to streamline embedded
development through an open source platform. Check out the :ref:`introduction` for more information.
All project ACRN source code is maintained in the https://github.com/projectacrn/acrn-hypervisor
repository and includes folders for the ACRN hypervisor, the ACRN device
model, tools, and documentation. You can either download this source code as
a zip or tar.gz file (see the `ACRN v1.3 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v1.3>`_)
or use Git clone and checkout commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v1.3

The project's online technical documentation is also tagged to correspond
with a specific release: generated v1.3 documents can be found at https://projectacrn.github.io/1.3/.
Documentation for the latest (master) branch is found at https://projectacrn.github.io/latest/.
ACRN v1.3 requires Clear Linux* OS version 31080. Please follow the
instructions in the :ref:`getting-started-apl-nuc`.

Version 1.3 major features
**************************

What's New in v1.3
==================
* OVMF supports Graphics Output Protocol (GOP), allowing Windows logo at guest
  VM boot time.
* Platform-level coordinated graceful shutdown (S5) for User VMs.
* Virtual UART (vUART) for inter-VM communication.
* Introduced :ref:`acrn-config <acrn_configuration_tool>` tool to configure VM
  and hypervisor from a XML configuration file at build time.
* Ethernet mediator now supports prioritization per VM.
* Features for real-time determinism, e.g. Cache Allocation Technology (CAT, only supported on Apollo Lake).

Document updates
================
We have many new `reference documents available <https://projectacrn.github.io>`_, including:

* :ref:`Getting Started Guide for Industry scenario <rt_industry_setup>`
* :ref:`ACRN Configuration Tool Manual <acrn_configuration_tool>`
* :ref:`Trace and Data Collection for ACRN Real-Time(RT) Performance Tuning <rt_performance_tuning>`
* :ref:`Building ACRN in Docker <building-acrn-in-docker>`
* :ref:`Running Ubuntu as the User VM <running_ubun_as_user_vm>`
* :ref:`Running Debian as the User VM <running_deb_as_user_vm>`
* :ref:`Running Debian as the Service VM <running_deb_as_serv_vm>`
* :ref:`vUART configuration <vuart_config>`
* :ref:`Enable virtio-i2c <virtio_i2c>`

New Features Details
********************

- :acrn-issue:`3624` - Virtualization supports Windows Guest Bootup Logo
- :acrn-issue:`3623` - Platform Configuration Tool
- :acrn-issue:`3602` - Hypervisor Tools
- :acrn-issue:`3624` - Virtualization supports Windows Guest Bootup Logo
- :acrn-issue:`3564` - Power Management: S5
- :acrn-issue:`3413` - Support NV storage writeback of OVMF
- :acrn-issue:`3327` - Cleanup vIOAPIC and vPIC for RT VM
- :acrn-issue:`3484` - tools: support force stop VM
- :acrn-issue:`3446` - Rename board name of nuc7i7bnh to nuc7i7dnb
- :acrn-issue:`3497` - Inject exception for invalid vmcall
- :acrn-issue:`3498` - Return extended info in vCPUID leaf 0x40000001
- :acrn-issue:`2934` - Use virtual APIC IDs for Pre-launched VMs
- :acrn-issue:`3459` - dm: support VMs communication with virtio-console           
- :acrn-issue:`3190` - DM: handle SIGPIPE signal

Fixed Issues Details
********************

- :acrn-issue:`3533` - NUC hang while repeating the cold boot
- :acrn-issue:`3572` - Check guest cr3 before loading pdptrs
- :acrn-issue:`3576` - Expand default memory from 2G to 4G for WaaG
- :acrn-issue:`3593` - Makefile change which add isd build
- :acrn-issue:`3594` - UOS have no response After ignore/poweroff/suspend with pressing power key
- :acrn-issue:`3609` - Sometimes fail to boot os while repeating the cold boot operation
- :acrn-issue:`3610` - LaaG hang while run some workloads loop with zephyr idle
- :acrn-issue:`3611` - OVMF launch UOS fail for Hybrid and industry scenario
- :acrn-issue:`3612` - Potential Null pointer be dereferenced in 'usb_dev_request()'
- :acrn-issue:`3626` - hv: vtd: fix MACRO typos
- :acrn-issue:`3644` - hv boot hang on some KBL platform
- :acrn-issue:`3648` - UOS hang when booting UOS with acrnlog running with mem loglevel=6
- :acrn-issue:`3708` - Properly reset pCPUs with LAPIC PT enabled during VM shutdown/reset

Known Issues
************

- :acrn-issue:`3598` - SEP/SOCWATCH fixes for following coding guidelines
- :acrn-issue:`3622` - Kernel PANIC while rebased acrngt patches to mainline kernel
- :acrn-issue:`3630` - Clean up the code on drm/i915/gvt
- :acrn-issue:`3636` - tsc_deadline incorrect issue
- :acrn-issue:`3673` - Incorrect reference to OVMF.fd in sample UOS startup script
- :acrn-issue:`3675` - cbm length calculation,Extended model judge, print info error
- :acrn-issue:`3681` - Data lose in vuart communication
- :acrn-issue:`3686` - The documentation build system creates artefacts in the
- :acrn-issue:`3697` - Secure timer check failed in trusty which would cause unlock failure after resume from S3
- :acrn-issue:`3721` - [Compiling Issue] Error implicit declaration with VIRTIO_PCI_CONFIG_OFF
- :acrn-issue:`3723` - CODEOWNERS folder names are incorrect
- :acrn-issue:`3729` - Cannot auto boot 2 VMs with acrnd

Change Log
**********

These commits have been added to the acrn-hypervisor repo since the v1.2
release in Aug 2019 (click on the CommitID link to see details):

.. comment

   This list is obtained from this git command (update the date to pick up
   changes since the last release):

   git log --pretty=format:'- :acrn-commit:`%h` - %s' --after="2019-08-23"

- :acrn-commit:`fe74464a` - doc: content updates for using SBL on UP2 board file
- :acrn-commit:`860f7b89` - doc: minor edits to using celadon as user os file
- :acrn-commit:`bb6d2acb` - doc: content updates to GSG for the Intel NUC
- :acrn-commit:`32614324` - doc: Add document of RT performance tuning.
- :acrn-commit:`ca27f8ed` - update using_sbl_on_up2 doc and create-up2-images.sh
- :acrn-commit:`36d52c71` - doc: Add note to use a stable Celadon source tree
- :acrn-commit:`72c99c08` - doc: update gsg and acrn_quick_setup.sh
- :acrn-commit:`e7d048b9` - doc: tweak vUART tutorial for improved rendering
- :acrn-commit:`8be09779` - Doc: Update image and text in Virtio-i2c doc
- :acrn-commit:`58a093de` - Doc: Initial vuart configuration doc and images--4979
- :acrn-commit:`6677add9` - Initial virtio i2c doc and images.
- :acrn-commit:`4692db8a` - New tutorials for running deb or ubunt as user vms
- :acrn-commit:`f2a32b48` - Intitial doc for Running Debian as a Service VM
- :acrn-commit:`d853c52d` - Language edits to the Building ACRN in Docker tutorial
- :acrn-commit:`68975ba7` - doc: add build acrn in docker tutorial
- :acrn-commit:`456709c6` - Makefile: build default acrn.efi with nuc6cayh
- :acrn-commit:`3e9f2aa3` - acrn-config: remove hvlog support for generic board
- :acrn-commit:`bd3a686d` - acrn-config: add apl-up2-n3350 config xmls
- :acrn-commit:`ac003623` - acrn-config: update board xml of apl-up2
- :acrn-commit:`55fbe8fb` - acrn-config: update README for acrn-config
- :acrn-commit:`77fa8650` - acrn-config: reserve 16M memory for hv start
- :acrn-commit:`f776cfd2` - acrn-config: fix parameter error in launch script
- :acrn-commit:`c15beff4` - Makefile: add build tag for acrn-config tool in version.h
- :acrn-commit:`c33a2c29` - Makefile: generate config patch in hypervisor/Makefile
- :acrn-commit:`571b30dc` - dm: switch to launch RT_LaaG with OVMF by default
- :acrn-commit:`bf971d75` - OVMF release v1.3
- :acrn-commit:`e5f733fb` - hv: vm: properly reset pCPUs with LAPIC PT enabled during VM shutdown/reset
- :acrn-commit:`bad75329` - acrn-config: add launch sample xmls for acrn-config
- :acrn-commit:`3c621ccc` - acrn-config: generate launch script file and apply to the souce file
- :acrn-commit:`bc31dc00` - acrn-config: modify rootfs tag in board information
- :acrn-commit:`f50f92cc` - Makefile: override .config with specified scenario
- :acrn-commit:`42b864b1` - DM: update launch scripts to use OVMF.fd directly from the rootfs
- :acrn-commit:`bfc92308` - DM: make LaaG launch script use the OVMF.fd from the Service VM
- :acrn-commit:`e0006883` - acrn-config: add memmap param for hvlog in xmls
- :acrn-commit:`b3ff3cdf` - HV: add memmap param for hvlog in sos cmdline
- :acrn-commit:`a348be73` - Misc: lifemngr-daemon-on-UOS
- :acrn-commit:`d2290076` - makefile: enable xml config to build hypervisor
- :acrn-commit:`6e122870` - acrn-config: add xmls for acrn-config tools
- :acrn-commit:`8a16d8b6` - acrn-config: generate a scenario patch and apply to acrn-hypervisor
- :acrn-commit:`77c17ab4` - acrn-config: enhance the target config
- :acrn-commit:`a95a88c5` - doc: Remove "or newer/higher" descriptions for other release notes and some tutorials.
- :acrn-commit:`12db54af` - doc: update CODEOWNER for rst
- :acrn-commit:`6b6aa806` - hv: pm: fix coding style issue
- :acrn-commit:`f039d759` - hv: pm: enhencement platform S5 entering operation
- :acrn-commit:`ce937587` - hv: pm: correct the function name
- :acrn-commit:`f41f9307` - DOC: add --pm_by_vuart setting guide.
- :acrn-commit:`3d23c90a` - DM: to avoid RTVM shutdown forcely by acrn-dm
- :acrn-commit:`8578125f` - DM: add power off by vuart setting to launch script
- :acrn-commit:`eb5a57b7` - DM: add guest vm power manager by vuart
- :acrn-commit:`00401a1e` - DM: separate pty vuart operation from IOC
- :acrn-commit:`d188afbc` - HV: add acpi info header for nuc7i7dnb
- :acrn-commit:`00da5a99` - acrn-config: web UI app for acrn-config tool
- :acrn-commit:`476e9a2e` - doc: Update document for --pm_notify_channel
- :acrn-commit:`e38e0263` - script: launch_uos: Give right pm notify channel
- :acrn-commit:`b36d80ea` - dm: pm: add dm option to select guest notify method
- :acrn-commit:`10413849` - dm: pm: move host power button related code out of pm.c
- :acrn-commit:`ca51cc9d` - hv: vPCI: vPCI device should use its virtual configure space to access its BAR
- :acrn-commit:`6ebc2221` - hv: vPCI: cache PCI BAR physical base address
- :acrn-commit:`5083aba3` - doc: review edits for config tool doc
- :acrn-commit:`ede59885` - doc: add 'logger_setting' parameter information to acrn-dm documentation
- :acrn-commit:`ff91d073` - doc: update .gitignore to reflect the new location of the tools
- :acrn-commit:`9bb21aca` - dm: remove '-p' option from the embedded help
- :acrn-commit:`8b9aa110` - hv: mmu: remove strict check for deleting page table mapping
- :acrn-commit:`127c73c3` - hv: mmu: add strict check for adding page table mapping
- :acrn-commit:`c691c5bd` - hv:add volatile keyword for some variables
- :acrn-commit:`be0c2a81` - doc: update CODEOWNERS for doc, misc reviews
- :acrn-commit:`26642543` - Merge pull request #3660 from deb-intel/3632_GCC
- :acrn-commit:`96d51a52` - Add URL to GCC 7.3 Manual, Section 6
- :acrn-commit:`639c6986` - dm: reserve 16M hole for gvt in e820 table
- :acrn-commit:`32d85105` - hv: remove pr_dbg between stac/clac
- :acrn-commit:`8d27c1e1` - Merge pull request #3632 from shiqingg/doc-lang-ext
- :acrn-commit:`19e9c4ca` - Merge pull request #3613 from gvancuts/acrnctl-force-arg
- :acrn-commit:`67f3da2e` - Merge pull request #3567 from ClaudZhang1995/zy4
- :acrn-commit:`876d3112` - Merge pull request #3640 from lirui34/add_new_glossary
- :acrn-commit:`ceec4d80` - Merge pull request #3649 from gvancuts/zlib1g-dev-debian
- :acrn-commit:`1b48773f` - Merge pull request #3653 from deb-intel/remove_newerRef
- :acrn-commit:`edbec46d` - doc: Add ACRN configuration tool tutorial
- :acrn-commit:`1e3da9f2` - Merge pull request #3658 from deb-intel/USBMed
- :acrn-commit:`fd60bb07` - Add supported USB devices for WaaG and LaaG OSs
- :acrn-commit:`5d284c08` - doc: Add three new glossaries
- :acrn-commit:`81435f55` - vm reset: refine platform reset
- :acrn-commit:`add89b51` - Remove "or newer" reference to ensure that users know ACRN 1.2 requires ONLY Clear Linux OS version 30690.
- :acrn-commit:`4041275f` - doc: update Build ACRN from Source
- :acrn-commit:`d324f79a` - doc: add 'zlib1g-dev' to list of dependencies in Debian
- :acrn-commit:`cd40980d` - hv:change function parameter for invept
- :acrn-commit:`1547a4cb` - efi-stub: fix stack memory free issue
- :acrn-commit:`cd1ae7a8` - hv: cat: isolate hypervisor from rtvm
- :acrn-commit:`38ca8db1` - hv:tiny cleanup
- :acrn-commit:`f15a3600` - hv: fix tsc_deadline correctness issue
- :acrn-commit:`3f84acda` - hv: add "invariant TSC" cap detection
- :acrn-commit:`be0a4b69` - DM USB: fix enumeration related issues
- :acrn-commit:`e7179aa7` - dm: support VM running with more than 4 vcpus
- :acrn-commit:`adf3a593` - Makefile: Refine Makefile to generate both industry and sdc images
- :acrn-commit:`3729fa94` - doc: update Language Extensions in coding guidelines
- :acrn-commit:`f9945484` - hv: vtd: fix MACRO typos
- :acrn-commit:`295701cc` - hv: remove mptable code for pre-launched VMs
- :acrn-commit:`b447ce3d` - hv: add ACPI support for pre-launched VMs
- :acrn-commit:`96b422ce` - hv: create 8-bit sum function
- :acrn-commit:`81e2152a` - hv: cosmetic fixes in acpi.h
- :acrn-commit:`216c19f4` - hv: use __packed for all ACPI related structs
- :acrn-commit:`a1ef0ab9` - hv: move ACPI related defines/structs to acpi.h
- :acrn-commit:`6ca4095d` - Update pages with missing links
- :acrn-commit:`cc1dd6da` - doc: add "-f/--force' optional arg to 'acrnctl' documentation
- :acrn-commit:`2d57c5fe` - dm: virtio-console: add subclass
- :acrn-commit:`66056c1a` - dm: bzimage loader: get linux bzimage setup_sects from header
- :acrn-commit:`fc3d19be` - DM USB: fix potential crash risk due to null pointer
- :acrn-commit:`4a71a16a` - hv: vtd: remove global cache invalidation per vm
- :acrn-commit:`5c816597` - hv: ept: flush cache for modified ept entries
- :acrn-commit:`2abd8b34` - hv: vtd: export iommu_flush_cache
- :acrn-commit:`826aaf7b` - version: 1.3-unstable

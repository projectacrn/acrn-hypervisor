.. _release_notes_2.2:

ACRN v2.1 (Sep 2020)
####################

We are pleased to announce the release of ACRN version 2.2


ACRN is a flexible, lightweight reference hypervisor, built with real-time and safety-criticality in mind, optimized to streamline embedded development through an open source platform. Check out the What is ACRN for more information. All project ACRN source code is maintained in the https://github.com/projectacrn/acrn-hypervisor repository and includes folders for the ACRN hypervisor, the ACRN device model, tools, and documentation. You can either download this source code as a zip or tar.gz file (see the ACRN v2.2 GitHub release page) or use Git clone and checkout commands:

git clone https://github.com/projectacrn/acrn-hypervisor
cd acrn-hypervisor
git checkout v2.2
The project’s online technical documentation is also tagged to correspond with a specific release: generated v2.2 documents can be found at https://projectacrn.github.io/2.2/. Documentation for the latest (master) branch is found at https://projectacrn.github.io/latest/. ACRN v2.2 requires Ubuntu 18.04. Please follow the instructions in the Getting Started Guide for ACRN Industry Scenario with Ubuntu Service VM.



Version 2.2 major features
What’s New in v2.2


Elkhart Lake and Tiger Lake processor support: At Intel Industrial Summit 2020, Intel announced the latest additions to their enhanced-for-IoT Edge portfolio: the Intel® Atom® x6000E Series, Intel® Pentium® and Intel®Celeron® N and J Series (all codenamed Elkhart Lake), and 11th Gen Intel® Core™ processors (codename Tiger Lake-UP3). The ACRN team is pleased to announce that this ACRN v2.2 release already supports these processors.

Support for time deterministic applications with new features e.g., Time Coordinated Computing and Time Sensitive Networking
Support for functional safety with new features e.g., Intel Safety Island
ACRN can boot on Slim Bootloader on Elkhart Lake, an alternative bootloader to UEFI BIOS.

Shared memory based inter-VM communication (ivshmem) is now extended to support all kinds of VMs including pre-launched VM, Service VM and other guest VMs.

CPU sharing supports pre-launched VM

RTLinux with preempt-RT linux kernel 5.4 is validated as a pre-launched VM as well as a post-launched VM.

ACRN hypervisor can emulate MSI-X based on physical MSI with multiple vectors.

Staged removal of deprivileged boot mode support: ACRN used deprivileged boot mode to meet Clearlinux’s special requirement, which is actually bringing scalability effort and not suitable for hybrid mode support. So ACRN is moving to support Grub/multiboot2, a much simpler way, which is also more scalable. Consequently, depriviledged boot mode is no longer the default in ACRN v2.2 and will be completely removed in v2.3

Document updates
New or updated reference documents are available, including:


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
* :ref:`using_yp`
* :ref:`acrn-dm_parameters`
* :ref:`hv-parameters`
* :ref:`acrnctl`

We're dropping deprivileged boot mode support in the next v2.3 release, and switching our Service VM of choice away from Clear Linux. We began this transition in the v2.2 documentation and removed some Clear Linux-specific tutorials.  Deleted documents are still available in the version-specific v2.1 documentation.

* tutorials/acrn-dm_QoS.rst was deleted.
* tutorials/acrn_ootb.rst was deleted.
* tutorials/agl-vms.rst was deleted.
* tutorials/building_acrn_in_docker.rst was deleted.
* tutorials/building_uos_from_clearlinux.rst was deleted.
* tutorials/cl_servicevm.rst was deleted.
* tutorials/enable_laag_secure_boot.rst was deleted.
* tutorials/increase-uos-disk-size.rst was deleted.
* tutorials/kbl-nuc-sdc.rst was deleted.
* tutorials/open_vswitch.rst was deleted.
* tutorials/running_deb_as_serv_vm.rst was deleted.
* tutorials/sign_clear_linux_image.rst was deleted.
* tutorials/static-ip.rst was deleted.
* tutorials/up2.rst was deleted.
* tutorials/using_celadon_as_uos.rst was deleted.
* tutorials/using_sbl_on_up2.rst was deleted.
* tutorials/using_ubuntu_as_sos.rst was deleted.

Fixed Issues Details
********************
- :acrn-issue:`5008` -  Slowdown in UOS (Zephyr)
- :acrn-issue:`5033` -  SOS decode instruction failed in hybrid mode
- :acrn-issue:`5038` -  [WHL][Yocto] SOS occasionally hangs/crashes with a kernel panic
- :acrn-issue:`5048` -  - iTCO_wdt issue: can't request region for resource -
- :acrn-issue:`5102` -  Can't acces shared memory base adress in ivshmem
- :acrn-issue:`5118` -  GPT ERROR when write preempt img to sata on NUC7i5BNB
- :acrn-issue:`5148` -  dm: support to provide ACPI SSDT for UOS
- :acrn-issue:`5157` -  [build from source] during build HV with XML, “TARGET_DIR=xxx” does not work
- :acrn-issue:`5165` -  [WHL][Yocto][YaaG] No UI display when launch Yaag gvt-g with acrn kernel
- :acrn-issue:`5215` -  [UPsquared N3350 board] Solution to Bootloader issue
- :acrn-issue:`5233` -  Boot Acrn failed on Dell-OptiPlex 5040 with Intel i5-6500T
- :acrn-issue:`5238` -  acrn-config: add hybrid_rt scenario xml config for ehl-crb-b
- :acrn-issue:`5240` -  passthru DHRD-ignored device
- :acrn-issue:`5242` -  acrn-config: add pse-gpio to vmsix_on_msi devices list
- :acrn-issue:`4691` -  hv: add vgpio device model support
- :acrn-issue:`5245` -  hv: add INTx mapping for pre-launched VMs
- :acrn-issue:`5426` -  hv: add vgpio device model support
- :acrn-issue:`5257` -  hv: support PIO access to platform hidden devices
- :acrn-issue:`5278` -  [EHL][acrn-configuration-tool]: create a new hybrid_rt based scenario for P2SB MMIO pass-thru use case
- :acrn-issue:`5304` -  Cannot cross-compile - Build process assumes build system always hosts the ACRN hypervisor

Known Issues
************
- :acrn-issue:`5150` - [REG][WHL][[Yocto][Passthru] Launch RTVM fails with usb passthru
- :acrn-issue:`5151` - [WHL][VxWorks] Launch VxWorks fails due to no suitable video mode found
- :acrn-issue:`5154` - [TGL][Yocto][PM] 148213_PM_SystemS5 with life_mngr fail
- :acrn-issue:`5368` - [TGL][Yocto][Passthru] Audio does not work on TGL
- :acrn-issue:`5369` - [TGL][qemu] Cannot launch qemu on TGL
- :acrn-issue:`5370` - [TGL][RTVM][PTCM] Launch RTVM failed with mem size smaller than 2G and PTCM enabled
- :acrn-issue:`5371` - [TGL][Industry][Xenomai]Xenomai post launch fail

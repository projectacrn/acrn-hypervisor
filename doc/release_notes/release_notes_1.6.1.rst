.. _release_notes_1.6.1:

ACRN v1.6.1 (May 2020)
######################

We are pleased to announce the release of ACRN version 1.6.1.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline
embedded development through an open source platform. Check out :ref:`introduction` for more information.
All project ACRN source code is maintained in the https://github.com/projectacrn/acrn-hypervisor
repository and includes folders for the ACRN hypervisor, the ACRN device
model, tools, and documentation. You can either download this source code as
a zip or tar.gz file (see the `ACRN v1.6.1 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v1.6.1>`_)
or use Git clone and checkout commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v1.6.1

The project's online technical documentation is also tagged to correspond
with a specific release: generated v1.6.1 documents can be found at
https://projectacrn.github.io/1.6.1/.
Documentation for the latest (master) branch is found at https://projectacrn.github.io/latest/.
ACRN v1.6.1 requires Clear Linux OS version 33050. Follow the
instructions in the :ref:`rt_industry_setup`.

Version 1.6.1 major features
****************************

What's New in v1.6.1
====================
* ACRN ensures libvirt supports VM orchestration based on OpenStack

   - libvirt is an open-source API, daemon, and management tool as a
     layer to decouple orchestrators and hypervisors.
     By adding a "ACRN driver", libvirt can support ACRN. Please refer to ACRN-libvirt.

   - Supports the libvirt-based orchestrator to configure a guest
     domain's CPU configuration during VM creation.

   - Supports dynamic configuration for vCPU affinity via acrn-dm

   - ACRN configuration tool updated based on VM orchestration support.

* Enable CPU sharing and GVT-d Graphics virtualization by default.

* Supported platforms with multiple IO-APICs

* Supported VT-d Posted Interrupts

Document updates
================
Many new and updated `reference documents <https://projectacrn.github.io>`_ are available, including:

* :ref:`asa`
* :ref:`setup_openstack_libvirt`
* :ref:`rt_perf_tips_rtvm`
* :ref:`acrn_configuration_tool`
* :ref:`hv-device-passthrough`
* :ref:`cpu_sharing`
* :ref:`getting-started-building`
* :ref:`rt_industry_setup`
* :ref:`using_windows_as_uos`

We recommend that all developers upgrade to ACRN release v1.6.1.

Fixed Issues Details
********************
- :acrn-issue: '1773'- [APLNUC][IO][LaaG]USB Mediator USB3.0 and USB2.0 flash disk boot up UOS, quickly hot plug USB and Can not recognize all the devices
- :acrn-issue: '3291'- Update documentation and helper scripts to use newer `swupd` commands.
- :acrn-issue: '3697'- Secure timer check failed in trusty which would cause unlock failure after resume from S3
- :acrn-issue: '3715'- Add support for multiple RDT resource allocation and fix L3 CAT config overwrite by L2
- :acrn-issue: '3758'- Documentation: add a tutorial (or information) on how to change the Linux kernel parameters for User VMs
- :acrn-issue: '3770'- Warning when building the ACRN hypervisor `SDC (defined at arch/x86/Kconfig:7) set more than once`
- :acrn-issue: '3773'- [Community][Internal] suspicious logic in vhost.c.
- :acrn-issue: '3918'- Change active_hp_work position for code cleaning and add a module parameter to disable hp work.
- :acrn-issue: '3939'- [Community][Internal]zero-copy non-functional with vhost.
- :acrn-issue: '3946'- [Community][External]Cannot boot VxWorks as UOS on KabyLake.
- :acrn-issue: '4017'- hv: rename vuart operations
- :acrn-issue: '4072'- [Community-dev][External]hv: add printf "not support the value of vuart index parameter" in function vuart_register_io_handler.
- :acrn-issue: '4191'- [Community-dev][External]acrnboot: the end address of _DYNAME region is not calculated correct
- :acrn-issue: '4200'- In APCIv advanced mode, a target vCPU (in not-root mode) may get wrong TMR or EOI exit bitmap when another vPCU try to send an interrupt to it if this interrupt trigger mode has changed.
- :acrn-issue: '4250'- [Community-dev][external]acrnboot: parse hv cmdline incorrectly when containing any trailing white-spaces
- :acrn-issue: '4283'- [Community-dev][External]devicemodel: refactor CMD_OPT_LAPIC_PT case branch
- :acrn-issue: '4322'- [ACRN_V1.5][Document] Build cmd error in "Build the ACRN User VM PREEMPT_RT Kernel in Docker" document
- :acrn-issue: '4569'- [acrn-configuration-tool]find 64-bit mmio to generate HI_MMIO_START/HI_MMIO_END
- :acrn-issue: '4620'- [WHL][Function][WaaG] WaaG will fail to reboot with 2 cores.
- :acrn-issue: '4625'- [WHL][ConfigurationTool][WAAG] Need to support passthrough GVT to WaaG by default
- :acrn-issue: '4634'- [acrn-configuration-tool]move new_board_config to board_def config and support to parse it
- :acrn-issue: '4636'- compile crashlog error with latest clearlinux
- :acrn-issue: '4641'- [WHL][acrn-configuration-tool]error "board/scenario xml not match" for created scenario setting
- :acrn-issue: '4664'- Wake up vCPU for interrupts from vPIC
- :acrn-issue: '4666'- Fix offline tool to generate info in pci_dev file for logical partition scenario
- :acrn-issue: '4688'- [WHL][acrn-configuration-tool] RELEASE=n does not take effect while using xml to make hypervisor
- :acrn-issue: '4719'- [WHL][Function][LaaG]Garbage display when shutdown LaaG with CPU sharing GVT-D
- :acrn-issue: '4752'- [WHL][acrn-configuration-tool] console loglevel is not changed if building hypervisor by xml
- :acrn-issue: '4753'- [KBLNUCi7][libvirt][HV] in acrn.efi which enable 4vcpu for laag, with libvirtd.service enabled, SOS kernel panic and reboot


Known Issues
************
- :acrn-issue:`4046` - [WHL][Function][WaaG] Error info popoup when run 3DMARK11 on Waag
- :acrn-issue:`4047` - [WHL][Function][WaaG] passthru usb, Windows will hang when reboot it
- :acrn-issue:`4313` - [WHL][VxWorks] Failed to ping when VxWorks passthru network
- :acrn-issue:`4557` - [WHL][Performance][WaaG] Failed to run 3D directX9 during Passmark9.0 performance test with 7212 gfx driver
- :acrn-issue:`4558` - [WHL][Performance][WaaG] WaaG reboot automatically during run 3D directX12 with 7212 gfx driver

Change Log
**********

These commits have been added to the acrn-hypervisor repo since the v1.6
release in Mar 2020 (click the CommitID link to see details):

.. comment

   This list is obtained from this git command (update the date to pick up
   changes since the last release):

   git log --pretty=format:'- :acrn-commit:`%h` - %s' --after="2020-04-02"

- :acrn-commit:`5632dead` - doc: update release_1.6 docs with master docs
- :acrn-commit:`ac5facd2` - doc: update CPU affinity related descriptions
- :acrn-commit:`14366380` - acrn-config: fix log macros for board defconfig
- :acrn-commit:`627dd1c8` - acrn-config: add clearlinux UOS for launch config xmls
- :acrn-commit:`67728c67` - ACRN/DM: Initialize the igd_lpc bridge to ISA_BRIDGE to make Linux guest happy
- :acrn-commit:`71479793` - acrn-config: assign PCPU0~3 to post vm by default
- :acrn-commit:`c390ab01` - hv: don't overwrite the statically configured vm_configs[] in hypercall
- :acrn-commit:`cbaf3e78` - acrn-dm: fix corner cases in acrn_parse_cpu_affinity()
- :acrn-commit:`d661d444` - acrn-config: refine slot assignment for launch config
- :acrn-commit:`eb47f8f5` - acrn-config: refinement for CPU affinity check
- :acrn-commit:`e8d00c2c` - local_gpa2hpa: INVALID_GPA also means failure of address conversion
- :acrn-commit:`440385d5` - ACRN/DM: Reset the passthrough device to fix garbage display issue
- :acrn-commit:`77b7721f` - DM USB: xHCI: Drop commands if the slot is disabled
- :acrn-commit:`16e33b30` - acrn-config: add vm type sanity check
- :acrn-commit:`11959829` - acrn-config: refinement for pci_devs in scenario config xmls
- :acrn-commit:`fb5c35d1` - acrn-config: parse cpu_affinity from launch config xmls
- :acrn-commit:`8cbc6199` - acrn-config: add cpu_affinity for launch config xmls
- :acrn-commit:`b9865fdf` - acrn-dm: change command option name from "pcpu_list" to "cpu_affinity"
- :acrn-commit:`a6ea34bc` - hv: Enable accessed bit in EPT paging
- :acrn-commit:`c72d1936` - acrn-config: update cpu_affinity in scenrio configuration xml files
- :acrn-commit:`cce7389d` - acrn-config: change names for vcpu_affinity[] related items
- :acrn-commit:`45cc2c5e` - acrn-dm: implement cpu_affinity command line argument
- :acrn-commit:`0805eb9a` - hv: dynamically configure CPU affinity through hypercall
- :acrn-commit:`46753944` - hv: replace vcpu_affinity array with cpu_affinity_bitmap
- :acrn-commit:`40ae32f1` - hv: provide vm_config information in get_platform_info hypercall
- :acrn-commit:`42c43993` - hv: some coding refinement in hypercall.c
- :acrn-commit:`c9fa9c73` - hv: move error message logging into gpa copy APIs
- :acrn-commit:`b9a7cf3b` - acrn-config: assign VM IDs for dynamic scenario and launch setting
- :acrn-commit:`bcfbc13f` - acrn-config: add attributes for scenario and launch setting
- :acrn-commit:`3799b95b` - acrn-config: add max VM count check when generating scenario XML file
- :acrn-commit:`1d4b7ab8` - acrn-config: refine template xmls
- :acrn-commit:`ea0c62da` - acrn-config: add 2 UUIDs for post-launched Standard VM
- :acrn-commit:`093b1c48` - acrn-config: add SOS_IDLE for SOS cmdline
- :acrn-commit:`88bed66e` - HV: refine usage of idle=halt in sos cmdline
- :acrn-commit:`510a0931` - Makefile: do not override RELEASE when build with XML
- :acrn-commit:`7410f9d0` - hv: vtd: fix potential dead loop if qi request timeout
- :acrn-commit:`bf917ae2` - acrn-config: Generate info in pci_dev file for Pre-Launched VMs
- :acrn-commit:`b99de16f` - hv: Wake up vCPU for interrupts from vPIC
- :acrn-commit:`75b59165` - acrn-config: remove sdc2 config xmls
- :acrn-commit:`08bcf4be` - acrn-config: refine the HV_RAM_SIZE/HV_RAM_START for board_defconfig
- :acrn-commit:`d7299604` - acrn-config: set HV_RAM_SIZE/HV_RAM_START to blank from config xmls
- :acrn-commit:`5e53ac03` - acrn-config: refine template xmls
- :acrn-commit:`718e7567` - acrn-config: modify epc_section to configurable="0"
- :acrn-commit:`49b3939e` - acrn-config: fix syntax for new logical partition xmls
- :acrn-commit:`fc3b4ed6` - acrn-config: refine GPU vpid format for launch script
- :acrn-commit:`d17076b4` - HV: remove sdc2 scenario support
- :acrn-commit:`7f1c4422` - HV: support up to 7 post launched VMs for industry scenario
- :acrn-commit:`9a23bedd` - crashlog: fix build issue under latest clearlinux
- :acrn-commit:`d742be2c` - HV: Kconfig: enable CPU sharing by default
- :acrn-commit:`4c7ffeea` - acrn-config: add template xmls for dynamic config
- :acrn-commit:`0445c5f8` - acrn-config: dynamic configuration for scenario setting and launch setting
- :acrn-commit:`a12b746a` - acrn-config: remove hard code UUID from config xmls
- :acrn-commit:`86e467f6` - acrn-config: Use vm_type to instead load_type/uuid/severity in config
- :acrn-commit:`85630258` - acrn-config: support to parse pci_devs for pre launched vm
- :acrn-commit:`e0c75652` - acrn-config: add pass-thru PCI device for pre launched vm xmls
- :acrn-commit:`19032398` - acrn-config: remove 'scenario' dependency from acrn config tool
- :acrn-commit:`cc5c6421` - Makefile: disable KCONFIG_FILE when build from xml
- :acrn-commit:`7d173917` - Kconfig: remove MAX_KATA_VM_NUM
- :acrn-commit:`4388099c` - Kconfig: change scenario variable type to string
- :acrn-commit:`28bffa77` - HV: merge sos_pci_dev config to sos macro
- :acrn-commit:`d9c302ba` - HV: init vm uuid and severity in macro
- :acrn-commit:`b08dbd41` - HV: fix wrong gpa start of hpa2 in ve820.c
- :acrn-commit:`60178a9a` - hv: maintain a per-pCPU array of vCPUs and handle posted interrupt IRQs
- :acrn-commit:`a07c3da3` - hv: define posted interrupt IRQs/vectors
- :acrn-commit:`f5f307e9` - hv: enable VT-d PI for ptdev if intr_src->pid_addr is non-zero
- :acrn-commit:`c9dd310e` - hv: check if the IRQ is intended for a single destination vCPU
- :acrn-commit:`198b2576` - hv: add function to check if using posted interrupt is possible for vm
- :acrn-commit:`1bc76991` - hv: extend union dmar_ir_entry to support VT-d posted interrupts
- :acrn-commit:`8be6c878` - hv: pass pointer to functions
- :acrn-commit:`cc5bc34a` - hv: extend struct pi_desc to support VT-d posted interrupts
- :acrn-commit:`b7a126cd` - hv: move pi_desc related code from vlapic.h/vlapic.c to vmx.h/vmx.c/vcpu.h
- :acrn-commit:`8e2efd6e` - hv: rename vlapic_pir_desc to pi_desc
- :acrn-commit:`233577e4` - acrn-config: enable hv config for scenarion setting UI
- :acrn-commit:`c5cd7cae` - acrn-config: add hv configurations to scenario config xmls
- :acrn-commit:`4a98f533` - acrn-config: add support to parse board defconfig from configurations
- :acrn-commit:`d0beb7e9` - acrn-config: support passthroug GVT for WaaG by default
- :acrn-commit:`1bf3163d` - hv: Hypervisor access to PCI devices with 64-bit MMIO BARs
- :acrn-commit:`910d93ba` - hv: Add HI_MMIO_START and HI_MMIO_END macros to board files
- :acrn-commit:`5e8fd758` - acrn-config: round HI_MMIO_START/HI_MMIO_END to the closest 1G
- :acrn-commit:`b9229348` - hv: fix for waag 2 core reboot issue
- :acrn-commit:`45b65b34` - hv: add lock for ept add/modify/del
- :acrn-commit:`bbdf0199` - hv: vpci: refine comment for pci_vdev_update_vbar_base
- :acrn-commit:`dad7fd80` - hv: Fix issues with the patch to reserve EPT 4K pages after boot
- :acrn-commit:`4bdcd33f` - hv: Reserve space for VMs'  EPT 4k pages after boot
- :acrn-commit:`963b8cb9` - hv: Server platforms can have more than 8 IO-APICs
- :acrn-commit:`4626c915` - hv: vioapic init for SOS VM on platforms with multiple IO-APICs
- :acrn-commit:`f3cf9365` - hv: Handle holes in GSI i.e. Global System Interrupt for multiple IO-APICs
- :acrn-commit:`ec869214` - hv: Introduce Global System Interrupt (GSI) into INTx Remapping
- :acrn-commit:`b0997e76` - hv: Pass address of vioapic struct to register_mmio_emulation_handler
- :acrn-commit:`9e21c5bd` - hv: Move error checking for hypercall parameters out of assign module
- :acrn-commit:`37eb369f` - hv: Use ptirq_lookup_entry_by_sid to lookup virtual source id in IOAPIC irq entries
- :acrn-commit:`0c9628f6` - acrn-config: remove the same parameters and functions from launch_cfg_lib
- :acrn-commit:`7d827c4d` - acrn-config: remove the same parameters and functions from scenario_cfg_lib
- :acrn-commit:`8e3ede1a` - acrn-config: remove the same parameters and functions from board_cfg_lib
- :acrn-commit:`df4a395c` - acrn-config: expends parameters and functions to common lib
- :acrn-commit:`6bbc5711` - acrn-config: Fixes for BAR remapping logic
- :acrn-commit:`889c0fa4` - acrn-config: update IOMEM_INFO of tgl-rvp board
- :acrn-commit:`bce6a3c4` - Makefile: support make with external configurations
- :acrn-commit:`3774244d` - Makefile: parameters check for board and scenario
- :acrn-commit:`82e93b77` - Makefile: make hypervisor from specified Kconfig
- :acrn-commit:`f8abeb09` - hv: config: enable RDT for apl-up2 by default
- :acrn-commit:`14e7f7a8` - acrn-config: enable CAT for industry scenario on APL-UP2 by default
- :acrn-commit:`02fea0f2` - acrn-config: support generation of per vcpu clos configuraton
- :acrn-commit:`76943866` - HV: CAT: support cache allocation for each vcpu
- :acrn-commit:`d18fd5f8` - acrn-config: find 64-bit mmio for HI_MMIO_START/HI_MMIO_END
- :acrn-commit:`d9d50461` - acrn-config: update IOMEM_INFO of native board config xml
- :acrn-commit:`e7726944` - acrn-config: dump iomem info from /proc/iomem
- :acrn-commit:`8e7b80fc` - acrn-config: Limit check on Pre-Launched VM RAM size
- :acrn-commit:`aa6bb9e2` - acrn-config: support '--out' option for board/scenario/launch config
- :acrn-commit:`05e3ea5f` - acrn-config: correct passthru 'audio' device for nuc6cayh
- :acrn-commit:`c980b360` - acrn-config: minor fix for generating CONFIG_PCI_BDF
- :acrn-commit:`6f8a7ba5` - acrn-config: add some configs in board defconfig
- :acrn-commit:`2eb8e0f7` - acrn-config: remove git check and avoid to generate patch for config files
- :acrn-commit:`48fdeb25` - acrn-config: one button to generate config file
- :acrn-commit:`ab879407` - acrn-config: create temporary scenario file folder if it doesn't exist


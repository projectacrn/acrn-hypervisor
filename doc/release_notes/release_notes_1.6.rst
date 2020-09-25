.. _release_notes_1.6:

ACRN v1.6 (Mar 2020)
####################

We are pleased to announce the release of ACRN version 1.6.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline
embedded development through an open source platform. Check out :ref:`introduction` for more information.
All project ACRN source code is maintained in the https://github.com/projectacrn/acrn-hypervisor
repository and includes folders for the ACRN hypervisor, the ACRN device
model, tools, and documentation. You can either download this source code as
a zip or tar.gz file (see the `ACRN v1.6 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v1.6>`_)
or use Git clone and checkout commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v1.6

The project's online technical documentation is also tagged to correspond
with a specific release: generated v1.6 documents can be found at https://projectacrn.github.io/1.6/.
Documentation for the latest (master) branch is found at https://projectacrn.github.io/latest/.
ACRN v1.6 requires Clear Linux OS version 32680.

Version 1.6 major features
**************************

What's New in v1.6
==================
* Graphics passthrough support

  - The hypervisor and Service VMs support allowing passthrough graphics devices to target DM-launched guest VMs, based on GVT-d.

* SRIOV support

  - The ACRN hypervisor allows an SRIOV-capable PCI device's Physical Function (PF) to be allocated to the Service VM.

  - The ACRN hypervisor allows a SRIOV-capable PCI device's Virtual Functions (VFs) to be allocated to any VM.

  - The ACRN Service VM supports the SRIOV Ethernet device (through the PF driver), and ensures that the SRIOV VF device is able to be assigned (passthrough) to a post-launched VM (launched by ACRN-DM).

* CPU sharing enhancement -  Halt/Pause emulation

  - For a vCPU that uses the fairness CPU scheduler, the hypervisor supports yielding an idle vCPU (when it's running a 'HLT' or 'PAUSE' instruction).

* PCI Config space access emulation for passthrough devices in the hypervisor

  - The hypervisor provides the necessary emulation (such as config space) of the passthrough PCI device during runtime for a DM-launched VM. Such runtime emulation is DM-independent.

* PCI bridge emulation in hypervisor

Document updates
================
Many new and updated `reference documents <https://projectacrn.github.io>`_ are available, including:

* :ref:`asa`
* :ref:`sriov_virtualization`
* :ref:`using_xenomai_as_uos`
* :ref:`split-device-model`
* :ref:`rdt_configuration`


We recommend that all developers upgrade to ACRN release v1.6.

New Features Details
********************

- :acrn-issue:`3381` - Avoid interference: PCI bridges
- :acrn-issue:`3831` - Pass-thru PCI device in DM-launched VM
- :acrn-issue:`4282` - Graphics passthrough
- :acrn-issue:`4329` - CPU Sharing: Halt/Pause Emulation
- :acrn-issue:`4360` - Graphics passthrough
- :acrn-issue:`4433` - SRIOV for ethernet device
- :acrn-issue:`4433` - SRIOV Physical Function Allocated To Service OS VM
- :acrn-issue:`4433` - SRIOV Any Virtual Function Allocated To Any One VM under sharing mode

Fixed Issues Details
********************
- :acrn-issue:`3465` -[SIT][ISD] [AUTO]add reset in"-s 2,passthru,02/00/0 \", rtvm can not launch
- :acrn-issue:`3789` -[Security][apl_sdc_stable]DM:The return value of snprintf is improperly checked.
- :acrn-issue:`3886` -Lapic-pt vcpu notification issue
- :acrn-issue:`4032` -Modify License file.
- :acrn-issue:`4042` -[KBL][HV]RTVM UOS result is invalid when run cpu2017 with 3 and 1 core
- :acrn-issue:`4094` -Error parameter for intel_pstate in launch_hard_rt_vm.sh
- :acrn-issue:`4175` -acrntrace fixes and improvement
- :acrn-issue:`4194` -Prevent compiler from optimizing out security checks
- :acrn-issue:`4212` -[KBL][acrn-configuration-tool][WaaG+RTVM]Need support pm_channel&pm_by_vuart setting for Board:nuc7i7dnb+WaaG&RTVM
- :acrn-issue:`4229` -Add range check in Kconfig.
- :acrn-issue:`4230` -Remove MAX_VCPUS_PER_VM in Kconfig
- :acrn-issue:`4253` -[WHL][Function][WaaG]Meet error log and waag can't boot up randomly after allocated 3 cores cpu to waag
- :acrn-issue:`4255` -[acrn-configuration-tool][nuc7i7dnb][sdc]uos has no ip address
- :acrn-issue:`4258` -[Community][External]cyclictest benchmark UOS getting high.
- :acrn-issue:`4282` -ACRN-DM Pass-thru devices bars prefetchable property isn't consistent with physical bars
- :acrn-issue:`4286` -[acrn-configuration-tool] Remove VM1.vcpu_affinity.pcuid=3 for VM1 in sdc scenario
- :acrn-issue:`4298` -[ConfigurationTool] mac address is not added to the launch script
- :acrn-issue:`4301` -[WHL][Hybrid] WHL need support Hybrid mode
- :acrn-issue:`4310` -[ISD][Function][WaaG] WaaG reboot automatically when run 3DMark-v1.5.915.0
- :acrn-issue:`4316` -[KataContainers]LaaG miss ip address when we create kata_container first with macvtap driver.
- :acrn-issue:`4325` -Do not wait pcpus offline when lapic pt is disabled.
- :acrn-issue:`4402` -UEFI UP2 board boot APs failed with ACRN hypervisor
- :acrn-issue:`4419` -[WHL][hybrid] SOS can not poweroff & reboot in hybrid mode of WHL board (multiboot2)
- :acrn-issue:`4472` -[WHL][sdc2] HV launch fails with sdc2 scenario which support launching 3 Guest OS
- :acrn-issue:`4492` -[acrn-configuration-tool] miss include head file from logical partition
- :acrn-issue:`4495` -[acrn-configuration-tool] Missing passthru nvme parameter while using WebUI to generate RTVM launch script

Known Issues
************
- :acrn-issue:`4046` - [WHL][Function][WaaG] Error info pop up when run 3DMARK11 on Waag
- :acrn-issue:`4047` - [WHL][Function][WaaG] passthru usb, Windows will hang when reboot it
- :acrn-issue:`4313` - [WHL][VxWorks] Failed to ping when VxWorks passthru network
- :acrn-issue:`4520` - efi-stub could get wrong bootloader name
- :acrn-issue:`4557` - [WHL][Performance][WaaG] Failed to run 3D directX9 during Passmark9.0 performance test with 7212 gfx driver
- :acrn-issue:`4558` - [WHL][Performance][WaaG] WaaG reboot automatically during run 3D directX12 with 7212 gfx driver
- :acrn-issue:`4560` - [WHL][SIT][HV]build HV fail in docker

Change Log
**********

These commits have been added to the acrn-hypervisor repo since the v1.5
release in Dec 2019 (click the CommitID link to see details):

.. comment

   This list is obtained from this git command (update the date to pick up
   changes since the last release):

   git log --pretty=format:`- :acrn-commit:`%h` - %s` --after="2019-12-18"

- :acrn-commit:`0aa2c237` - hv: change GPU passthru translation mode to TT_PASSTHROUGH
- :acrn-commit:`64352596` - hv: unmap SR-IOV VF MMIO when the VF physical device is disabled
- :acrn-commit:`1d7158c0` - acrn-config: fix missing passthru parameter for launch config
- :acrn-commit:`0eeab73c` - acrn-config: add missed include in pci_dev.c for logical partition
- :acrn-commit:`05dc6c53` - OVMF release v1.6
- :acrn-commit:`fd2330c9` - Doc: Changed lines in RN 1.0 and 0.1 to correct ref issue.
- :acrn-commit:`14692ef6` - hv:Rename two VM states
- :acrn-commit:`a5f9ef40` - Doc: Fix tool ref tag on develop.rst page
- :acrn-commit:`9a85e274` - Doc: Re-org documentation to improve user experience; see Nav Bar
- :acrn-commit:`b62d439b` - acrn-config: remove a function that generates ve820 file
- :acrn-commit:`27b6c82c` - acrn-config: keep HV_RAM_START 2M memory align
- :acrn-commit:`91b06a35` - acrn-config: remap PCI vbar address to high memory
- :acrn-commit:`830df76f` - acrn-config: refine VM number macro from scenario config
- :acrn-commit:`a8c2ba03` - HV: add pci_devices.h for nuc6cayh and apl-up2
- :acrn-commit:`a68f655a` - HV: update ept address range for pre-launched VM
- :acrn-commit:`e7455349` - HV: move create_sos_vm_e820 to ve820.c
- :acrn-commit:`a7b61d25` - HV: remove board specific ve820
- :acrn-commit:`d7eac3fe` - HV: decouple prelaunch VM ve820 from board configs
- :acrn-commit:`4c0965d8` - HV: correct ept page array usage
- :acrn-commit:`e9a99845` - hv: refine read/write configuration APIs for vmsi/vmsix
- :acrn-commit:`4b6dd19a` - hv: pci: rename CFG read/write function for PCI-compatible Configuration Mechanism
- :acrn-commit:`7e74ed55` - misc:life_mngr: support S5 triggered by RTVM
- :acrn-commit:`e641202c` - Doc: Add note to index.html file Note invites users to view v1.5 branch while latest build is under construction.
- :acrn-commit:`3743edf9` - doc: add site under construction page header
- :acrn-commit:`910ac9f9` - dm:send shutdown to life_mngr on SOS
- :acrn-commit:`f78558a4` - dm: add one api for sending shutdown to life_mngr on SOS
- :acrn-commit:`8733abef` - dm:handle shutdown command from UOS
- :acrn-commit:`4fdc2be1` - dm:replace shutdown_uos_thread with a new one
- :acrn-commit:`7e9b7a8c` - dm:set pm-vuart attributes
- :acrn-commit:`790614e9` - hv:rename several variables and api for ioapic
- :acrn-commit:`fa74bf40` - hv: vpci: pass through stolen memory and opregion memory for GVT-D
- :acrn-commit:`659e5420` - hv: add static check for CONFIG_HV_RAM_START and CONFIG_HV_RAM_SIZE
- :acrn-commit:`696f6c7b` - hv: the VM can only deinit its own devices
- :acrn-commit:`d8a19f99` - hv: refine naming
- :acrn-commit:`08ed45f4` - hv: fix wrong VF BDF
- :acrn-commit:`7b429fe4` - hv: prohibit PF from being assigned
- :acrn-commit:`657af925` - hv: passthrough a VF device
- :acrn-commit:`640cf57c` - hv: disable VF device
- :acrn-commit:`2a4235f2` - hv: refine function find_vdev
- :acrn-commit:`d67d0538` - hv: initialize VF BARs
- :acrn-commit:`ddd6253a` - hv: wrap msix map/unmap operations
- :acrn-commit:`41350c53` - hv: vpci: add _v prefix for some function name
- :acrn-commit:`835dc22a` - acrn-config: sdc2 UUID update
- :acrn-commit:`f727d1e7` - HV: sdc2 UUID update
- :acrn-commit:`60a7c49b` - hv: Refine code for API reduction
- :acrn-commit:`b25d5fa5` - acrn-config: remove redundant sos bootargs from vm config
- :acrn-commit:`e5ae37eb` - hv: mmu: minor fix about add_pte
- :acrn-commit:`43676577` - hv: vpci: add a global CFG header configuration access handler
- :acrn-commit:`460e7ee5` - hv: Variable/macro renaming for intr handling of PT devices using IO-APIC/PIC
- :acrn-commit:`9a794432` - acrn-config: Generate target xml and board.c file with MBA RDT resource
- :acrn-commit:`2aaa050c` - HV: move out physical cfg write from vpci-bridge
- :acrn-commit:`ad4d14e3` - HV: enable ARI if PCI bridge support it
- :acrn-commit:`b6684f5b` - HV: sanitize config file for whl-ipc-i5
- :acrn-commit:`64bf4fb8` - dm: don't deassign pass through PCIe device in DM
- :acrn-commit:`67cb1029` - hv: update the hypervisor 64-bit entry address for efi-stub
- :acrn-commit:`49ffe168` - hv: fixup relocation delta for symbols belong to entry section
- :acrn-commit:`2aa8c9e5` - hv: add multiboot2 tags to load relocatable raw binary
- :acrn-commit:`97fc0efe` - hv: remove unused cpu_primary_save_32()
- :acrn-commit:`f0e5387e` - hv: remove pci_vdev_read_cfg_u8/16/32
- :acrn-commit:`e1ca1ae2` - hv: refine functions name
- :acrn-commit:`7c82efb9` - hv: pci: add some pre-assumption and safety check for PCIe ECAM
- :acrn-commit:`667639b5` - doc: fix a missing argument in the function description
- :acrn-commit:`93fa2bc0` - hv: minor fixes in init_paging()
- :acrn-commit:`734ad6ce` - hv: refine pci_read_cap and pci_read_ext_cap
- :acrn-commit:`76f2e28e` - doc: update hv device passthrough document
- :acrn-commit:`b05c1afa` - doc: add doxygen style comments to ptdev
- :acrn-commit:`b6c0558b` - HV: Update existing board.c files for RDT MBA
- :acrn-commit:`92ee33b0` - HV: Add MBA support in ACRN
- :acrn-commit:`d54d35ef` - acrn-config: correct console argument for logical partition scenario
- :acrn-commit:`d54deca8` - hv: initialize SRIOV VF device
- :acrn-commit:`176cb31c` - hv: refine vpci_init_vdev function
- :acrn-commit:`320ed6c2` - hv: refine init_one_dev_config
- :acrn-commit:`87e7d791` - hv: refine init_pdev function
- :acrn-commit:`abbdef4f` - hv: implement SRIOV VF_BAR initialization
- :acrn-commit:`298ef2f5` - hv: refine init_vdev_pt function
- :acrn-commit:`58c0a474` - acrn-config: Fix vbar address generated by the offline tool
- :acrn-commit:`cee8dc22` - acrn-config: Remove "GUEST_FLAG_CLOS_REQUIRED" from offline tool
- :acrn-commit:`984c0753` - xmls: Update existing <$BOARD$>.xml files for RDT support
- :acrn-commit:`a81fcc23` - acrn-config: Set/Unset RDT support in the <$BOARD$>.config file
- :acrn-commit:`6cfd81cd` - acrn-config: Generate board.c file with multiple RDT resources
- :acrn-commit:`b9f46943` - acrn-config: Update common platform clos max on scenario and vm configuration
- :acrn-commit:`cdac28e8` - acrn-config: Update platform max CLOS value to be least common value among RDT resources.
- :acrn-commit:`89a63543` - acrn-config: Extract RDT resource and CLOS from target xml file
- :acrn-commit:`4a007cc3` - acrn-config: Generate target xml file with multiple RDT resources
- :acrn-commit:`a63f8109` - dm: avoid clear guest memory content if guest is RTVM
- :acrn-commit:`be1e3acb` - dm: remove vdev_update_bar_map callback for PCIe device
- :acrn-commit:`595cefe3` - hv: xsave: move assembler to individual function
- :acrn-commit:`2f748306` - hv: introduce SRIOV interception
- :acrn-commit:`14931d11` - hv: add SRIOV capability read/write entries
- :acrn-commit:`5e989f13` - hv: check if there is enough room for all SRIOV VFs.
- :acrn-commit:`ac147795` - hv: implement SRIOV-Capable device detection.
- :acrn-commit:`c751a8e8` - hv: refine confusing e820 table logging layout
- :acrn-commit:`bd92304d` - HV: add vpci bridge operations support
- :acrn-commit:`c246d1c9` - hv: xsave: bugfix for init value
- :acrn-commit:`96f92373` - hv:refine comment about intel integrated gpu dmar
- :acrn-commit:`3098c493` - acrn-config: avoid conflict slot for launch config
- :acrn-commit:`0427de5e` - acrn-config: Kata VM is not supported on dual-core systems
- :acrn-commit:`cef3322d` - HV: Add WhiskeyLake board configuration files
- :acrn-commit:`eaad91fd` - HV: Remove RDT code if CONFIG_RDT_ENABLED flag is not set
- :acrn-commit:`d0665fe2` - HV: Generalize RDT infrastructure and fix RDT cache configuration.
- :acrn-commit:`887e3813` - HV: Add both HW and SW checks for RDT support
- :acrn-commit:`b8a021d6` - HV: split L2 and L3 cache resource MSR
- :acrn-commit:`25974299` - HV: Rename cat.c/.h files to rdt.c/.h
- :acrn-commit:`ee455574` - doc: update copyright year in doc footer
- :acrn-commit:`b2c6cf77` - hv: refine retpoline speculation barriers
- :acrn-commit:`da3d181f` - HV: init efi info with multiboot2
- :acrn-commit:`69da0243` - HV: init module and rsdp info with multiboot2
- :acrn-commit:`b669a719` - HV: init mmap info with multiboot2
- :acrn-commit:`d008b72f` - HV: add multiboot2 header info
- :acrn-commit:`19ffaa50` - HV: init and sanitize acrn multiboot info
- :acrn-commit:`520a0222` - HV: re-arch boot component header
- :acrn-commit:`708cae7c` - HV: remove DBG_LEVEL_PARSE
- :acrn-commit:`a46a7b35` - Makefile: Fix build issue if the ld is updated to 2.34
- :acrn-commit:`ad606102` - hv: sched_bvt: add tick handler
- :acrn-commit:`77c64ecb` - hv: sched_bvt: add pick_next function
- :acrn-commit:`a38f2cc9` - hv: sched_bvt: add wakeup and sleep handler
- :acrn-commit:`e05eb42c` - hv: sched_bvt: add init and deinit function
- :acrn-commit:`a7563cb9` - hv: sched_bvt: add BVT scheduler
- :acrn-commit:`64b874ce` - hv: rename BOOT_CPU_ID to BSP_CPU_ID
- :acrn-commit:`4adad73c` - hv: mmio: refine mmio access handle lock granularity
- :acrn-commit:`fbe57d9f` - hv: vpci: restrict SOS access assigned PCI device
- :acrn-commit:`9d3d9c3d` - dm: vpci: restrict SOS access assigned PCI device
- :acrn-commit:`e8479f84` - hv: vPCI: remove passthrough PCI device unused code
- :acrn-commit:`9fa6eff3` - dm: vPCI: remove passthrough PCI device unused code
- :acrn-commit:`dafa3da6` - vPCI: split passthrough PCI device from DM to HV
- :acrn-commit:`aa38ed5b` - dm: vPCI: add assign/deassign PCI device IC APIs
- :acrn-commit:`fe3182ea` - hv: vPCI: add assign/deassign PCI device HC APIs
- :acrn-commit:`2ca01206` - Makefile: fix build issue on old gcc
- :acrn-commit:`f3a4b232` - hv: add P2SB device to whitelist for apl-mrb
- :acrn-commit:`170aa935` - acrn-config: add P2SB device to whitelist for apl-mrb
- :acrn-commit:`0829edee` - dm:add an extra lpc bridge when enabling gvt-d
- :acrn-commit:`da2ed57a` - dm:add igd-lpc class for Windows guest when enabling gvt-d
- :acrn-commit:`1303861d` - hv:enable gpu iommu except APL platforms
- :acrn-commit:`1f1eb7fd` - hv:disable iommu snoop control to enable gvt-d by an option
- :acrn-commit:`53de3a72` - hv: reset vcpu events in reset_vcpu
- :acrn-commit:`cf3544b4` - Doc: VM2 vCPU affinity info update
- :acrn-commit:`cc6f0949` - hv: CAT is supposed to be enabled in the system level
- :acrn-commit:`8dcede76` - Makefile: disable fcf-protection for some build env
- :acrn-commit:`8ddbfc26` - acrn: add pxelinux as known bootloader
- :acrn-commit:`50f28452` - acrn-config: a few changes on vm_config[] clos generation
- :acrn-commit:`7f57e64e` - Delete pass-through audio to WaaG in default.
- :acrn-commit:`7d4b2c82` - Edits to Ubuntu SOS; changed SOS/UOS to Service VM/User VM
- :acrn-commit:`f3249e77` - hv: enable early pr_xxx() logs
- :acrn-commit:`db6fe1e3` - doc: update Grub configuration instructions for Ubuntu (Service VM)
- :acrn-commit:`920f0270` - acrn: rename param in uart16550_init
- :acrn-commit:`2e10930d` - Python scripts discovering CPU IDs are off by 1
- :acrn-commit:`ef1c92e8` - fix typos in script
- :acrn-commit:`8896ba25` - Grammatical edits to Run Kata Containers doc
- :acrn-commit:`88dfd8d4` - doc: update Kata and ACRN tutorial
- :acrn-commit:`e1eedc99` - Doc: Style updates to Building from Source doc
- :acrn-commit:`1f6c0cd4` - doc: update project's target max LOC
- :acrn-commit:`8f9e4c2d` - Updated grammar in ACRN industry scenario doc
- :acrn-commit:`54e9b562` - doc: Modify CL version from 32030 to 31670
- :acrn-commit:`1b3754aa` - dm:passthrough opregion to uos gpu
- :acrn-commit:`4d882731` - dm:passthrough graphics stolen memory to uos gpu
- :acrn-commit:`f9f64d35` - dm:reserve 64M hole for graphics stolen memory in e820 table
- :acrn-commit:`10c407cc` - HV: init local variable before it is used.
- :acrn-commit:`086e0f19` - hv: fix pcpu_id mask issue in smp_call_function()
- :acrn-commit:`a631c94c` - doc: reset clear linux version and ootb command in getting start guide
- :acrn-commit:`dbf9b933` - doc: update the "Using SDC Mode on the NUC" tutorial
- :acrn-commit:`809338a3` - Doc: Clarify Post-Launch VM data flow discussion for vuart conf
- :acrn-commit:`fd4775d0` - hv: rename VECTOR_XXX and XXX_IRQ Macros
- :acrn-commit:`b9086292` - hv: rename the ACRN_DBG_XXX
- :acrn-commit:`03f5c639` - dm:derive the prefetch property of PCI bar for pass-through device
- :acrn-commit:`ceb197c9` - dm:keep pci bar property unchanged when updating pci bar address
- :acrn-commit:`b59e5a87` - hv: Disable HLT and PAUSE-loop exiting emulation in lapic passthrough
- :acrn-commit:`3edde260` - hv: debug: show vcpu thread status in vcpu_list debug command
- :acrn-commit:`db708fc3` - hv: rename is_completion_polling to is_polling_ioreq
- :acrn-commit:`e4f5c1ef` - version: 1.6-unstable
- :acrn-commit:`008c35a8` - Doc: Updated one paragraph in re_industry doc.
- :acrn-commit:`7cef407d` - Doc: Fixed spelling error in the acrn_config_tool file.
- :acrn-commit:`02ce44ce` - Doc: Style and grammar edits to GSG for ACRN Industry Scenario.
- :acrn-commit:`e8512bf7` - Doc: Grammar updates to ACRN Config Tool doc.
- :acrn-commit:`54511773` - doc: update some xml elements description
- :acrn-commit:`9e244b1b` - doc: update getting start guide about clear linux version and ootb commands
- :acrn-commit:`5f1fa3cf` - doc: change version info
- :acrn-commit:`82b89fd0` - hv: check the validity of `pdev` in `set_ptdev_intr_info`
- :acrn-commit:`fe03d870` - Doc: releasenotes_1.5 update
- :acrn-commit:`e91ecaa7` - Doc: Grammar update to arcn_ootd.rst.
- :acrn-commit:`88644ab7` - Doc: document update base on release_v1.5
- :acrn-commit:`5267a977` - dm:replace perror with pr_err
- :acrn-commit:`0e47f0a8` - hv: fix potential NULL pointer reference in hc_assgin_ptdev
- :acrn-commit:`ddebefb9` - hv: remove depreciated code for hc_assign/deassign_ptdev
- :acrn-commit:`96aba9bd` - Doc: Grammatical edits to RN 1.5.
- :acrn-commit:`9b454dc4` - Doc: releasenotes_1.5
- :acrn-commit:`65ed6c35` - hv: vpci: trap PCIe ECAM access for SOS
- :acrn-commit:`1e50ec88` - hv: pci: use ECAM to access PCIe Configuration Space
- :acrn-commit:`57a36206` - acrn-config: set up whitelist for board containing hide pci device
- :acrn-commit:`65f3751e` - hv: pci: add hide pci devices configuration for apl-up2
- :acrn-commit:`3239cb0e` - hv: Use HLT as the default idle action of service OS
- :acrn-commit:`4303ccb1` - hv: HLT emulation in hypervisor
- :acrn-commit:`a8f6bdd4` - hv: Add vlapic_has_pending_intr of apicv to check pending interrupts
- :acrn-commit:`e3c30336` - hv: vcpu: wait and signal vcpu event support
- :acrn-commit:`1f23fe3f` - hv: sched: simple event implementation
- :acrn-commit:`4115dd62` - hv: PAUSE-loop exiting support in hypervisor.
- :acrn-commit:`bfecf30f` - HV: do not offline pcpu when lapic pt disabled.
- :acrn-commit:`c59f12da` - doc: fix wrong Docker container image in tutorial.
- :acrn-commit:`41a998fc` - hv: cr: handle control registers related to PCID.
- :acrn-commit:`4ae350a0` - hv: vmcs: pass-through instruction INVPCID to VM.
- :acrn-commit:`d330879c` - hv: cpuid: expose PCID related capabilities to VMs.
- :acrn-commit:`96331462` - hv: vmcs: remove redundant check on vpid.
- :acrn-commit:`5f2c303a` - acrn-config: dump CPU info from /sys/devices/system/cpu/possible.
- :acrn-commit:`5d1a08fc` - Doc: Added missing period in run_kata_containers file.
- :acrn-commit:`9071349a` - doc: Update some of the wrong path in acrn configuration tool doc.
- :acrn-commit:`e25a2bf8` - doc: add more details to the Kata Containers with ACRN tutorial.
- :acrn-commit:`933e2178` - dm: pci: reset passthrough device by default.
- :acrn-commit:`21b405d1` - hv: vpci: an assign PT device should support FLR or PM reset.
- :acrn-commit:`e74a9f39` - hv: pci: add PCIe PM reset check.
- :acrn-commit:`26670d7a` - hv: vpci: revert do FLR and BAR restore.
- :acrn-commit:`6c549d48` - hv: vpci: restore physical BARs when writing Command Register if necessary.
- :acrn-commit:`742abaf2` - hv: add sanity check for vuart configuration.
- :acrn-commit:`c6f7803f` - HV: restore lapic state and apic id upon INIT.
- :acrn-commit:`ab132285` - HV: ensure valid vcpu state transition.
- :acrn-commit:`a5158e2c` - HV: refine reset_vcpu api.
- :acrn-commit:`d1a46b82` - HV: rename function of vlapic_xxx_write_handler.
- :acrn-commit:`9ecac862` - HV: clean up redundant macro in lapic.h.
- :acrn-commit:`46ed0b15` - HV: correct apic lvt reset value.
- :acrn-commit:`d4bf019d` - Doc: Added Whiskey Lake specs to hardware ref page.
- :acrn-commit:`8a8438df` - remove no support OS parts and add whl build.
- :acrn-commit:`58b3a058` - hv: vpci: rename pci_bar to pci_vbar.
- :acrn-commit:`d2089889` - hv: pci: minor fix of coding style about pci_read_cap.
- :acrn-commit:`cdf9d6b3` - (ia) devicemodel: refactor CMD_OPT_LAPIC_PT case branch.
- :acrn-commit:`77c3ce06` - acrn-config: remove unnecessary split for `virtio-net`
- :acrn-commit:`ce35a005` - acrn-config: add `cpu_sharing` support for launch config.
- :acrn-commit:`3544f7c8` - acrn-config: add `cpu_sharing` info in launch xmls.
- :acrn-commit:`57939730` - HV: search rsdp from e820 acpi reclaim region.
- :acrn-commit:`fc78013f` - acrn-config: some cleanup for logical partition mode Linux bootargs.
- :acrn-commit:`8f9cda18` - DOC: Content edits to CPU Sharing doc.
- :acrn-commit:`651510a8` - acrn-config: add `logger_setting` into launch script.
- :acrn-commit:`7f74e6e9` - acrn-config: refine mount device for virtio-blk.
- :acrn-commit:`fc357a77` - acrn-config: add `tap_` prefix for virtio-net.
- :acrn-commit:`5b6a33bb` - acrn-config: enable log_setting in all VMs.
- :acrn-commit:`d4bf019d` - Doc: Added Whiskey Lake specs to hardware ref page.
- :acrn-commit:`8a8438df` - remove no support OS parts and add whl build.
- :acrn-commit:`58b3a058` - hv: vpci: rename pci_bar to pci_vbar.
- :acrn-commit:`d2089889` - hv: pci: minor fix of coding style about pci_read_cap.
- :acrn-commit:`cdf9d6b3` - (ia) devicemodel: refactor CMD_OPT_LAPIC_PT case branch.
- :acrn-commit:`77c3ce06` - acrn-config: remove unnecessary split for `virtio-net`
- :acrn-commit:`ce35a005` - acrn-config: add `cpu_sharing` support for launch config.
- :acrn-commit:`3544f7c8` - acrn-config: add `cpu_sharing` info in launch xmls.
- :acrn-commit:`57939730` - HV: search rsdp from e820 acpi reclaim region.
- :acrn-commit:`fc78013f` - acrn-config: some cleanup for logical partition mode Linux bootargs.
- :acrn-commit:`8f9cda18` - DOC: Content edits to CPU Sharing doc.
- :acrn-commit:`651510a8` - acrn-config: add `logger_setting` into launch script.
- :acrn-commit:`7f74e6e9` - acrn-config: refine mount device for virtio-blk.
- :acrn-commit:`fc357a77` - acrn-config: add `tap_` prefix for virtio-net.
- :acrn-commit:`5b6a33bb` - acrn-config: enable log_setting in all VMs.
- :acrn-commit:`bb6e28e1` - acrn-config: check pass-through device for audio/audio_codec.
- :acrn-commit:`4234d2e4` - acrn-config: correct vuart1 setting in scenario config.
- :acrn-commit:`d80a0dce` - acrn-config: fix a few formatting issues.
- :acrn-commit:`051f277c` - acrn-config: modify hpa start size value for logical_partition scenario.
- :acrn-commit:`e5117bf1` - vm: add severity for vm_config.
- :acrn-commit:`f7df43e7` - reset: detect highest severity guest dynamically.
- :acrn-commit:`bfa19e91` - pm: S5: update the system shutdown logical in ACRN.
- :acrn-commit:`197e4a06` - acrn-config: add support to parse `severity` item tag.
- :acrn-commit:`ca2855f2` - acrn-config: add severity setting to scenario config xml.
- :acrn-commit:`a4085538` - Doc: Content edits to Running Kata containers on a Service VM doc.
- :acrn-commit:`9ee55965` - Doc: More edits to CPU Sharing doc.
- :acrn-commit:`fcb85a80` - acrn-config: remove pcpu3 from vm1 in SDC scenario
- :acrn-commit:`1fddf943` - hv: vpci: restore PCI BARs when doing AF FLR
- :acrn-commit:`a90e0f6c` - hv: vpci: restore PCI BARs when doing PCIe FLR
- :acrn-commit:`3c2f4509` - Doc: Add v1.5 release menu choice.
- :acrn-commit:`3e45d5e3` - Doc: Content edit to cpu-sharing page.
- :acrn-commit:`fa5922c8` - Doc: Content edit to rt_industry document.
- :acrn-commit:`17f6344c` - doc: Add tutorial about how to launch kata vm.
- :acrn-commit:`2ceff270` - doc: modify Configuration Tools
- :acrn-commit:`7edf8ed7` - doc: add document for cpu sharing

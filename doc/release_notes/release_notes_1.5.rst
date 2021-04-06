.. _release_notes_1.5:

ACRN v1.5 (Jan 2020)
####################

We are pleased to announce the release of ACRN version 1.5.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline embedded
development through an open source platform. Check out the :ref:`introduction` for more information.
All project ACRN source code is maintained in the https://github.com/projectacrn/acrn-hypervisor
repository and includes folders for the ACRN hypervisor, the ACRN device
model, tools, and documentation. You can either download this source code as
a zip or tar.gz file (see the `ACRN v1.5 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v1.5>`_)
or use Git clone and checkout commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v1.5

The project's online technical documentation is also tagged to correspond
with a specific release: generated v1.5 documents can be found at https://projectacrn.github.io/1.5/.
Documentation for the latest (master) branch is found at https://projectacrn.github.io/latest/.
ACRN v1.5 requires Clear Linux* OS version 32030.

Version 1.5 Major Features
**************************

What's New in v1.5
==================
* Basic CPU sharing: Fairness Round-Robin CPU Scheduling has been added to support basic CPU sharing (the Service VM and WaaG share one CPU core).
* 8th Gen Intel® Core™ Processors (code name Whiskey Lake) are now supported and validated.
* Overall stability and performance has been improved.
* An offline configuration tool has been created to help developers port ACRN to different hardware boards.

Document Updates
================
Many new `reference documents <https://projectacrn.github.io>`_ are available, including:

* :ref:`run-kata-containers`
* :ref:`hardware` (Addition of Whiskey Lake information)
* :ref:`cpu_sharing`
* :ref:`using_windows_as_uos` (Update to use ACRNGT GOP to install Windows)

Fixed Issues Details
********************

- :acrn-issue:`3630` - Clean up the code on drm/i915/gvt.
- :acrn-issue:`3723` - CODEOWNERS folder names are incorrect.
- :acrn-issue:`3777` - Tutorial "Using partition mode on UP2" no longer valid.
- :acrn-issue:`3779` - Modify the Make file.
- :acrn-issue:`3795` - fix a bug that tpr threshold is not updated
- :acrn-issue:`3830` - [KBLNUC][WaaG] bring back non-context register save/restore
- :acrn-issue:`3903` - Local variable 'info' maybe referenced before initializing in 'vmsi_remap()'
- :acrn-issue:`3904` - remove registration of default port IO and MMIO handlers
- :acrn-issue:`3930` - [APL][acrn-configuration-tool][AaaG]Generate unnecessary runc_container code for apl-mrb/apl-up2 with Scenario:SDC + Launch Setting:sdc_launch_1uos_aaag
- :acrn-issue:`3931` - [APL][acrn-configuration-tool][LaaG]Generated Launch script is incorrect, UOS's rootfs_img should be clearlinux.img|android.img for apl-mrb/apl-up2 with Scenario:SDC
- :acrn-issue:`3953` - Modify efi of default build.
- :acrn-issue:`3955` - [KBL][acrn-configuration-tool]'virtio-blk' was generated in launch script of Preempt-RT, should remove it
- :acrn-issue:`3956` - [KBL][acrn-configuration-tool]'keep_gsi' should set along with android vm
- :acrn-issue:`3960` - [Community][External]UP2 Setup - "ACRN HVLog: not running under acrn hypervisor!"
- :acrn-issue:`3968` - Modify efi of default build.
- :acrn-issue:`3972` - doc: no need to copy OVMF.fd to local folder while preparing the User VMs
- :acrn-issue:`3979` - [KBLNUC][WaaG][GVT]The boot uos script will display the sos information on the uos screen for about 2s.
- :acrn-issue:`3980` - [Community][External]invalid ovmf param ./OVMF.fd BOARD=nuc7i7dnb.
- :acrn-issue:`3984` - [Community][External]Fedora 30 as User OS.
- :acrn-issue:`3987` - No bounds specified for array platform_clos_array .
- :acrn-issue:`3993` - trampoline code in hypervisor potentially be accessible to service VM
- :acrn-issue:`4005` - [WHL][Function][WaaG]Fail to create WaaG image using ISO only on WHL
- :acrn-issue:`4007` - V1.3 E2E release binary failed to boot up on KBL NUC with 32G memory.
- :acrn-issue:`4010` - [Community][External]Booting in blind mode
- :acrn-issue:`4012` - Error formatting flag for hypcall_id
- :acrn-issue:`4020` - Refine print string format for 'uint64_t' type value in hypervisor
- :acrn-issue:`4043` - [WHL][Function][WaaG]windows guest can not get normal IP after passthru Ethernet
- :acrn-issue:`4045` - [WHL][Function][WaaG]Adding USB mediator in launch script, it takes a long time to start windows, about 13 minutes.
- :acrn-issue:`4049` - [SIT][ISD] [AUTO] only 2 can work in"-s n,passthru,02/00/0 \", other numbers rtvm can not launch
- :acrn-issue:`4061` - Some scripts are missing license and copyright header
- :acrn-issue:`4066` - [UP2][KBL]][acrn-configuration-tool] head file was not included in board.c
- :acrn-issue:`4073` - [APL-MRB][acrn-configuration-tool] alloc vuar1 irq when pttyS1 not exist only
- :acrn-issue:`4074` - [KBL][acrn-configuration-tool]: Cx desc parsing enhancement
- :acrn-issue:`4082` - [acrn-configuration-tool]bypass acpi_idle/acpi_cpufreq driver
- :acrn-issue:`4094` - Error parameter for intel_pstate in launch_hard_rt_vm.sh
- :acrn-issue:`4099` -[Community][External]Boot issue on non Apollo/Kaby lake.
- :acrn-issue:`4116` - [Community][External]How to set CPU Core UOS
- :acrn-issue:`4123` - [Community][External]Creating Ubuntu SOS not working - black screen
- :acrn-issue:`4125` - [Community][External]vm1 is running, can't create twice!
- :acrn-issue:`4128` - [WHL][acrn-configuration-tool]WebUI can not select vuart 0&vuart 1 by default
- :acrn-issue:`4135` - [Community][External]Invalid guest vCPUs (0) Ubuntu as SOS.
- :acrn-issue:`4139` - [Community][External]mngr_client_new: Failed to accept from fd 38
- :acrn-issue:`4143` - [acrn-configuration-tool] bus of DRHD scope devices is parsed incorrectly
- :acrn-issue:`4163` - [acrn-configuration-tool] not support: -s n,virtio-input
- :acrn-issue:`4164` - [acrn-configuration-tool] not support: -s n,xhci,1-1:1-2:2-1:2-2
- :acrn-issue:`4165` -[WHL][acrn-configuration-tool]Configure epc_section is incorrect
- :acrn-issue:`4172` - [acrn-configuration-tool] not support: -s n,virtio-blk, (/root/part.img---dd if=/dev/zero of=/root/part.img bs=1M count=10  all/part of img, one u-disk device, u-disk as rootfs and the n is special)
- :acrn-issue:`4173` - [acrn-configuration-tool]acrn-config tool not support parse default pci mmcfg base
- :acrn-issue:`4175` - acrntrace fixes and improvement
- :acrn-issue:`4185` - [acrn-configuration-tool] not support: -s n,virtio-net, (not set,error net, set 1 net, set multi-net, vhost net)
- :acrn-issue:`4211` - [kbl nuc] acrn failed to boot when generate hypervisor config source from config app with HT enabled in BIOS
- :acrn-issue:`4212` - [KBL][acrn-configuration-tool][WaaG+RTVM]Need support pm_channel&pm_by_vuart setting for Board:nuc7i7dnb+WaaG&RTVM
- :acrn-issue:`4227` - [ISD][Stability][WaaG][Regression] "Passmark8.0-Graphics3D-DirectX9Complex" test failed on WaaG due to driver error
- :acrn-issue:`4228` - [acrn-configuration-tool] cannot boot hypervisor on customer board with KBL 7300U
- :acrn-issue:`4229` - Add range check in Kconfig.
- :acrn-issue:`4230` - Remove MAX_VCPUS_PER_VM in Kconfig
- :acrn-issue:`4232` - Set default KATA_VM_NUM to 1 for SDC
- :acrn-issue:`4247` - [acrn-configuration-tool] Generate Scenario for VM0 communities with VM1 is incorrect.
- :acrn-issue:`4249` - [acrn-configuration-tool]Generated Launchscript but WebUI prompt error msg after we just select passthru-devices:audio_codec
- :acrn-issue:`4255` - [acrn-configuration-tool][nuc7i7dnb][sdc]uos has no ip address
- :acrn-issue:`4260` - [Community][External]webcam switch between 2 UOS.
- :acrn-issue:`4286` - [acrn-configuration-tool] Remove VM1.vcpu_affinity.pcuid=3 for VM1 in sdc scenario

Known Issues
************
- :acrn-issue:`4047` - passthru usb, when WaaG boot at "windows boot manager" menu, the usb keyboard does not work
- :acrn-issue:`4316` - [KataContainers]LaaG miss ip address when we create kata_container first with macvtap driver
- :acrn-issue:`4317` - [WHL][Function][WaaG]Mediator usb earphone, play audio will incontinuous and not clearly

Change Log
**********

These commits have been added to the acrn-hypervisor repo since the v1.4
release in Nov 2019 (click on the CommitID link to view details):

.. comment

   This list is obtained from this git command (update the date to pick up
   changes since the last release):

   git log --pretty=format:'- :acrn-commit:`%h` - %s' --after="2020-01-02"

- :acrn-commit:`ee74737f` - HV: search rsdp from e820 acpi reclaim region
- :acrn-commit:`578a7ab4` - acrn-config: remove pcpu3 from vm1 in SDC scenario
- :acrn-commit:`7d27c4bc` - hv: vpci: restore PCI BARs when doing AF FLR
- :acrn-commit:`bb06f6f9` - hv: vpci: restore PCI BARs when doing PCIe FLR
- :acrn-commit:`92ed8601` - hv: hotfix for xsave
- :acrn-commit:`067d8536` - OVMF release v1.5
- :acrn-commit:`9b71c5cd` - acrn-config: add 'logger_setting' into launch script
- :acrn-commit:`be6c6851` - acrn-config: refine mount device for virtio-blk
- :acrn-commit:`686d7763` - HV: Remove INIT signal notification related code
- :acrn-commit:`d7eb14c5` - HV: Use NMI to replace INIT signal for lapic-pt VMs S5
- :acrn-commit:`29b7aff5` - HV: Use NMI-window exiting to address req missing issue
- :acrn-commit:`d26d8bec` - HV: Don't make NMI injection req when notifying vCPU
- :acrn-commit:`24c2c0ec` - HV: Use NMI to kick lapic-pt vCPU's thread
- :acrn-commit:`23422713` - acrn-config: add `tap\_` prefix for virtio-net
- :acrn-commit:`6383394b` - acrn-config: enable log_setting in all vm
- :acrn-commit:`0b44d64d` - acrn-config: check pass-through device for audio/audio_codec
- :acrn-commit:`75ca1694` - acrn-config: correct vuart1 setting in scenario config
- :acrn-commit:`d52b45c1` - hv:fix crash issue when handling HC_NOTIFY_REQUEST_FINISH
- :acrn-commit:`78139b95` - HV: kconfig: add range check for memory setting
- :acrn-commit:`24994703` - HV: Kconfig: set default Kata num to 1 in SDC
- :acrn-commit:`9d5e72e9` - hv: add lock for ept add/modify/del
- :acrn-commit:`98b3dd94` - acrn-config: set HV_RAM_START above 256M for new board
- :acrn-commit:`46463900` - acrn-config: add 'ramdisk_mod' item tag for tgl-rvp
- :acrn-commit:`13d6b69d` - acrn-config: set DRHDx_IGNORE while no DEV_SCOPE in DRHD
- :acrn-commit:`12a9bc29` - acrn-config: add CONFIG_SERIAL_x for new board
- :acrn-commit:`d699347e` - acrn-config: change gvt_args from selectbox to editbox
- :acrn-commit:`05682b2b` - hv:bugfix in write protect page hypercall
- :acrn-commit:`1636ac04` - acrn-config: Add non-contiguous HPA to currently supported hardware.
- :acrn-commit:`2777f230` - HV: Add helper function send_single_nmi
- :acrn-commit:`525d4d3c` - HV: Install a NMI handler in acrn IDT
- :acrn-commit:`fb346a6c` - HV: refine excp/external_interrupt_save_frame and excp_rsvd
- :acrn-commit:`7f964654` - hv:remove need_cleanup flag in create_vm
- :acrn-commit:`67ec1b77` - HV: expose port 0x64 read for SOS VM
- :acrn-commit:`a44c1c90` - HV: Kconfig: remove MAX_VCPUS_PER_VM in Kconfig
- :acrn-commit:`0ba84348` - acrn-config: rename CONFIG_MAX_PCPU_NUM to MAX_PCPU_NUM
- :acrn-commit:`ea3476d2` - HV: rename CONFIG_MAX_PCPU_NUM to MAX_PCPU_NUM
- :acrn-commit:`67b416d5` - acrn-config: hide non-legacy serial port as SOS console
- :acrn-commit:`deb5ed1f` - acrn-config: unify get_vuart_info_id api in config tool
- :acrn-commit:`212d030b` - acrn-config: add 'poweroff_channel' support for launch config
- :acrn-commit:`7446d41f` - acrn-config: modify 'poweroff_channel' info in launch xmls
- :acrn-commit:`0f19f878` - acrn-config: add 'virtio-console' info in launch xmls
- :acrn-commit:`bad3c53c` - acrn-config: add 'virtio-console' mediator support for launch config
- :acrn-commit:`b6bffd01` - hv:remove 2 unused variables in vm_arch structure
- :acrn-commit:`422a051c` - Makefile: Build Release version by default
- :acrn-commit:`e95b316d` - hv: vtd: fix improper use of DMAR_GCMD_REG
- :acrn-commit:`68ea2cc6` - acrn-config: Fix ve820 table generation when guest memory size is >512MB
- :acrn-commit:`f2bf3d3e` - dm:gvt:update bus0 memlimit32 value
- :acrn-commit:`acb5affd` - doc:update acrn-shell.rst
- :acrn-commit:`413f098b` - Doc: Add libnuma dependency for acrntrace
- :acrn-commit:`a90f4a0a` - Makefile: print config summary at the end
- :acrn-commit:`9729fe07` - acrn-config: support non-contiguous HPA for hybrid scenario
- :acrn-commit:`c8a4ca6c` - HV: Extend non-contiguous HPA for hybrid scenario
- :acrn-commit:`b32ae229` - hv: sched: use hypervisor configuration to choose scheduler
- :acrn-commit:`6a144e6e` - hv: sched: add yield support
- :acrn-commit:`6554437c` - hv: sched_iorr: add some interfaces implementation of sched_iorr
- :acrn-commit:`b39630a8` - hv: sched_iorr: add tick handler and runqueue operations
- :acrn-commit:`f44aa4e4` - hv: sched_iorr: add init functions of sched_iorr
- :acrn-commit:`ed400863` - hv: sched_iorr: Add IO sensitive Round-robin scheduler
- :acrn-commit:`3c8d465a` - acrnboot: correct the calculation of the end boundary of _DYNAMIC region
- :acrn-commit:`0bf03b41` - acrntrace: Set FLAG_CLEAR_BUF by default
- :acrn-commit:`9e9e1f61` - acrntrace: Add opt to specify the cpus where we should capture the data
- :acrn-commit:`366f4be4` - acrntrace: Use correct format for total run time
- :acrn-commit:`1e192f05` - acrntrace: break when finding the matching key
- :acrn-commit:`9655b9de` - acrntrace: Fix the incorrect total vmexit cnt issue
- :acrn-commit:`1115c0c6` - acrn-config: UI supports to edit multiple virtio input devices.
- :acrn-commit:`557e7f19` - Makefile: add gcc flags to prevent some optimization
- :acrn-commit:`c2c05a29` - hv: vlapic: kick targeted vCPU off if interrupt trigger mode has changed
- :acrn-commit:`ed65ae61` - HV: Kconfig changes to support server platform.
- :acrn-commit:`706dbc0e` - acrn-config: support non-contiguous HPA for pre-launched VM
- :acrn-commit:`6e8b4136` - HV: Add support to assign non-contiguous HPA regions for pre-launched VM
- :acrn-commit:`9b44e57d` - acrn-config: Fix target xml generation issue when no P-state scaling driver is present
- :acrn-commit:`03a1b2a7` - hypervisor: handle reboot from non-privileged pre-launched guests
- :acrn-commit:`26801210` - Makefile: fix make failure for logical_partition or hybrid scenario
- :acrn-commit:`65a55320` - acrn-config: add xml to support TGL RVP board
- :acrn-commit:`1fe1afd4` - acrn-config: Add ramdisk tag parsing support
- :acrn-commit:`2b9fa856` - acrn-config: Add ramdisk tag to supported board/scenario xmls
- :acrn-commit:`da3ba68c` - hv: remove corner case in ptirq_prepare_msix_remap
- :acrn-commit:`c05d9f80` - hv: vmsix: refine vmsix remap
- :acrn-commit:`5f5ba1d6` - hv: vmsi: refine write_vmsi_cfg implementation
- :acrn-commit:`2f642002` - dm:gvt:enable gvt bar registration
- :acrn-commit:`89908bf5` - dm:gvt:update gvt bars before other pci devices write bar address
- :acrn-commit:`f27d4754` - dm:gvt:adjust pci bar region with reserved bar regions
- :acrn-commit:`1ac0b57c` - dm:gvt:reserve gvt bar regions in ACRN-DM
- :acrn-commit:`72644ac2` - hv: do not sleep a non-RUNNING vcpu
- :acrn-commit:`d624eb5e` - hv: io: do schedule in IO completion polling loop
- :acrn-commit:`d48da2af` - hv: bugfix for debug commands with smp_call
- :acrn-commit:`47139bd7` - hv: print current sched_object in acrn logmsg
- :acrn-commit:`5eb80402` - acrn-config: update UI to support virtio devices
- :acrn-commit:`5309e415` - acrn-config: modify the description of usb xhci
- :acrn-commit:`7838b537` - acrn-config: add virtio-net mediator support for launch config
- :acrn-commit:`25b2a26e` - acrn-config: add 'virtio-network' info in launch xmls
- :acrn-commit:`8464419a` - acrn-config: add virtio-block support for launch config
- :acrn-commit:`40140281` - acrn-config: add rootfs_dev/rootfs_img with virtio-blk item
- :acrn-commit:`aedd2c70` - acrntrace: parse leaf and subleaf of cpuid
- :acrn-commit:`aae974b4` - HV: trace leaf and subleaf of cpuid
- :acrn-commit:`77039f29` - acrn-config: Extend ve820 generation script for sizes gt 512 MB
- :acrn-commit:`450d2cf2` - hv: trap RDPMC instruction execution from any guest
- :acrn-commit:`3d412266` - hv: ept: build 4KB page mapping in EPT for RTVM for MCE on PSC
- :acrn-commit:`0570993b` - hv: config: add an option to disable mce on psc workaround
- :acrn-commit:`192859ee` - hv: ept: apply MCE on page size change mitigation conditionally
- :acrn-commit:`3cb32bb6` - hv: make init_vmcs as a event of VCPU
- :acrn-commit:`15da33d8` - HV: parse default pci mmcfg base
- :acrn-commit:`80a7281f` - acrn-config: add MMCFG_BASE_INFO item in board config
- :acrn-commit:`0e273e99` - acrn-config: get default pci mmcfg base address
- :acrn-commit:`0d998d6a` - hv: sync physical and virtual TSC_DEADLINE when msr interception enabled/disabled
- :acrn-commit:`97916364` - hv: fix virtual TSC_DEADLINE msr read/write issues
- :acrn-commit:`e6141298` - hv: support xsave in context switch
- :acrn-commit:`8ba203a1` - hv: change xsave init function name
- :acrn-commit:`12a3ec8a` - acrn-config: remove redundant get_leaf_tag_map in launch config lib
- :acrn-commit:`2c2ccfc5` - acrn-config: support OVMF vbootloader only
- :acrn-commit:`38a647c8` - acrn-config: correct epc_section base/size value
- :acrn-commit:`91330eaa` - acrn-config: add usb xhci mediator support for
- :acrn-commit:`420b65a6` - acrn-config: add 'usb_xhci' info to launch xmls
- :acrn-commit:`bc9b6d1b` - acrn-config: add virtio-input support for launch
- :acrn-commit:`9fc32043` - acrn-config: add 'virtio-input' info in launch xmls
- :acrn-commit:`71c51a8f` - acrn-config: refinement for library config
- :acrn-commit:`1e233364` - acrn-config: skip the DRHDn_IGNORE when no device scope
- :acrn-commit:`40929efe` - acrn-config: walk secondary PCI Bus for target board
- :acrn-commit:`5e923420` - acrn-config: refinement for DmarDevScope struct
- :acrn-commit:`f6e6ec4c` - acrn-config: modify SDC config xml to support kata vm config in webUI
- :acrn-commit:`bb2218ef` - acrn-config: add UI to add or remove Kata VM for sdc scenario
- :acrn-commit:`31d023e8` - acrn-config: launch refinement on vcpu affinity and uos image
- :acrn-commit:`d581473c` - acrn-config: refine vcpu affinity/number for SDC scenario
- :acrn-commit:`d44440f7` - acrn-config: print warning if MMIO BAR size above 4G
- :acrn-commit:`dc2d6b66` - acrn-config: modify the git commit message for gen_patch
- :acrn-commit:`2c4ebdc6` - hv: vmsi: name vmsi with verb-object style
- :acrn-commit:`6ee076f7` - hv: assign: rename ptirq_msix_remap to ptirq_prepare_msix_remap
- :acrn-commit:`51a43dab` - hv: add Kconfig parameter to define the Service VM EFI bootloader
- :acrn-commit:`058b03c3` - dm: fix memory free issue for xhci
- :acrn-commit:`422330d4` - HV: reimplement PCI device discovery
- :acrn-commit:`94a456ae` - HV: refactor device_to_dmaru
- :acrn-commit:`34c75a0b` - doc: Add multiple PCI segments as known limitation for hypervisor
- :acrn-commit:`c5a87d41` - HV: Cleanup PCI segment usage from VT-d interfaces
- :acrn-commit:`810169ad` - HV: initialize IOMMU before PCI device discovery
- :acrn-commit:`ea131eea` - HV: add DRHD index to pci_pdev
- :acrn-commit:`0b7bcd64` - HV: extra methods for extracting header fields
- :acrn-commit:`9af4a624` - doc: edit using_ubuntu_as_sos.rst adjust to v1.4
- :acrn-commit:`32b8d99f` - hv:panic if there is no memory map in multiboot info
- :acrn-commit:`bd0dbd27` - hv:add dump_guest_mem
- :acrn-commit:`215bb6ca` - hv:refine dump_host_mem
- :acrn-commit:`4c8dde1b` - hv:remove show_guest_call_trace
- :acrn-commit:`24fa14bc` - Revert "Revert "OVMF release v1.4""
- :acrn-commit:`5b4d676b` - version: 1.5-unstable


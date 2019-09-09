.. _release_notes_1.2:

ACRN v1.2 (Aug 2019)
####################

We are pleased to announce the release of ACRN version 1.2.

ACRN is a flexible, lightweight reference hypervisor, built with
real-time and safety-criticality in mind, optimized to streamline embedded
development through an open source platform. Check out the :ref:`introduction` for more information.
All project ACRN source code is maintained in the https://github.com/projectacrn/acrn-hypervisor
repository and includes folders for the ACRN hypervisor, the ACRN device
model, tools, and documentation. You can either download this source code as
a zip or tar.gz file (see the `ACRN v1.2 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v1.2>`_)
or use Git clone and checkout commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v1.2

The project's online technical documentation is also tagged to correspond
with a specific release: generated v1.2 documents can be found at https://projectacrn.github.io/1.2/.
Documentation for the latest (master) branch is found at https://projectacrn.github.io/latest/.
ACRN v1.2 requires Clear Linux* OS version 30690. Please follow the
instructions in the :ref:`getting-started-apl-nuc`.

Version 1.2 major features
**************************

What's New in v1.2
==================
* Support OVMF as virtual boot loader for Service VM to launch Clearlinux, VxWorks
  or Windows, Secure boot is supported
* Support Kata container
* Windows as a Guest (WaaG): USB host (xHCI) mediator
* Virtualization supports Always Running Timer (ART)
* Various bug fixes and enhancements

Document updates
================
We have many `reference documents available <https://projectacrn.github.io>`_, including:

* :ref:`Using Windows as User VM <using_windows_as_uos>`
* :ref:`How to sign binaries of the Clear Linux image <sign_clear_linux_image>`
* :ref:`Using Celadon as User VM <using_celadon_as_uos>`
* :ref:`SGX Virtualization <sgx_virt>`

We also updated the following documents based on the newly
defined **Usage Scenarios** in this release, including:

* :ref:`Introduction to Project ACRN <introduction>`
* :ref:`Build ACRN from Source <getting-started-building>`
* :ref:`Supported Hardware <hardware>`
* :ref:`Using Hybrid mode on NUC <using_hybrid_mode_on_nuc>`
* :ref:`Launch Two User VMs on NUC using SDC2 Scenario <using_sdc2_mode_on_nuc>`

New Features Details
********************

- :acrn-issue:`3051` - Kata Container Storage: Support rescan feature for virtio-blk
- :acrn-issue:`3163` - WaaG: USB host support
- :acrn-issue:`3401` - System is lagging when execute "fdisk -l" to query disk information with multiple USB devices after launch or reboot UOS
- :acrn-issue:`3486` - USB mediator: mediator mode can not recognize the SSD disk in RTVM and LaaG
- :acrn-issue:`3501` - Virtualization supports Always Running Timer (ART)
- :acrn-issue:`3505` - Succeed to reboot the whole system while execute reboot command in SOS
- :acrn-issue:`3506` - OVMF release v1.2

Fixed Issues Details
********************

- :acrn-issue:`2326` - There is no Read value output in the log when test "ST_PERF_USB2/3_mass_storage_protocol_Read_Large_File_as_Host"
- :acrn-issue:`2927` - The android guest will hung after test USB mediator camera[Frequency:20%]
- :acrn-issue:`3027` - Reboot SOS Failed[Frequency:10%]
- :acrn-issue:`3152` - Use virtio-blk instead passthru devices to boot RT
- :acrn-issue:`3181` - [auto][sit][daily]Case "Hypervisor_Launch_RTVM_on_SATA_Storage" sata disk can not passthru
- :acrn-issue:`3239` - HV can not produce #GP correctly sometimes
- :acrn-issue:`3268` - dm: add virtio-rnd device to command line
- :acrn-issue:`3277` - Potential Memory Leaks Found
- :acrn-issue:`3279` - AcrnGT causes display flicker in some situations
- :acrn-issue:`3280` - AcrnGT holding forcewake lock cause high cpu usage gvt workload thread
- :acrn-issue:`3281` - AcrnGT emulation thread causes high cpu usage when shadowing ppgtt
- :acrn-issue:`3283` - New scenario-based configurations lack documentation
- :acrn-issue:`3341` - Documentation on how to run Windows as a Guest (WaaG)
- :acrn-issue:`3370` - vm_console 2 cannot switch to VM2’s console in hybrid mode
- :acrn-issue:`3374` - Potential interrupt info overwrite in acrn_handle_pending_request
- :acrn-issue:`3379` - DM: Increase hugetlbfs MAX_PATH_LEN from 128 to 256
- :acrn-issue:`3392` - During run UnigenHeaven 3D gfx benchmark in WaaG, RTVM lantency is much long
- :acrn-issue:`3466` - Buffer overflow will happen in 'strncmp' when 'n_arg' is 0
- :acrn-issue:`3467` - Potential risk in virtioi_i2c.c & virtio_console.c
- :acrn-issue:`3469` - [APL NUC] Display goes black while booting; when only one display monitor is connected
- :acrn-issue:`3473` - dm: bugfix for remote launch guest issue
- :acrn-issue:`3480` - Add script to ``acrn-config/target`` and ``acrn-config/host/board_confing``
- :acrn-issue:`3482` - Acrn-hypervisor Root Directory Clean-up and Create misc/ folder for Acrn daemons, services and tools
- :acrn-issue:`3512` - hv: hypervisor console may hang in some platforms

Known Issues
************

:acrn-issue:`3465` - HV: reinit pbar base if a device reset is detected
   When a passthru pci device is reset, its physical bar base address may be reset to 0,
   with vpci bar emulation, vpci needs to reinit the physical bar base address to a
   valid address if a device reset is detected.

   **Impact:** Fail to launch Clear Linux Preempt_RT VM with ``reset`` passthru parameter

   **Workaround:** Issue resolved on ACRN tag: ``acrn-2019w33.1-140000p``

-----

:acrn-issue:`3520` - bundle of "VGPU unconformance guest" messages observed for "gvt" in SOS console while using UOS
   After the need_force_wake is not removed in course of submitting VGPU workload,
   it will print a bundle of below messages while the User VM is started.

   | gvt: vgpu1 unconformance guest detected
   | gvt: vgpu1 unconformance mmio 0x2098:0xffffffff,0x0

   **Impact:** Messy and repetitive output from the monitor

   **Workaround:** Need to rebuild and apply the latest Service VM kernel from the ``acrn-kernel`` source code.

-----

:acrn-issue:`3533` - NUC hang while repeating the cold boot
   NUC will hang while repeating cold boot operation.

   1) Before begin coldboot, enable no passwd ssh for SOS and RTVM.
   #) Boot up Service VM
   #) Boot up Zephyr guest with UUID "d2795438-25d6-11e8-864e-cb7a18b34643"
   #) Boot up RTVM with UUID "495ae2e5-2603-4d64-af76-d4bc5a8ec0e5"
   #) Reboot RTVM and then will restart the whole system
   #) After Service VM boot up, return to step 3

   **Impact:** Cold boot operation is not stable for NUC platform

   **Workaround:** Need to rebuild and apply the latest Service VM kernel from the ``acrn-kernel`` source code.

-----

:acrn-issue:`3576` - Expand default memory from 2G to 4G for WaaG

   **Impact:** More memory size is required from Windows VM

   **Workaround:** Issue resolved on ACRN tag: ``acrn-2019w33.1-140000p``

-----

:acrn-issue:`3609` - Sometimes fail to boot os while repeating the cold boot operation

   **Workaround:** Please refer the PR information in this git issue

-----

:acrn-issue:`3610` - LaaG hang while run some workloads loop with zephyr idle

   **Workaround:** Revert commit ``bbb891728d82834ec450f6a61792f715f4ec3013`` from the kernel

-----

:acrn-issue:`3611` - OVMF launch UOS fail for Hybrid and industry scenario

   **Workaround:** Please refer the PR information in this git issue

-----


Change Log
**********

These commits have been added to the acrn-hypervisor repo since the v1.1
release in June 2019 (click on the CommitID link to see details):

.. comment

   This list is obtained from this git command (update the date to pick up
   changes since the last release):

   git log --pretty=format:'- :acrn-commit:`%h` - %s' --after="2019-06-21"

- :acrn-commit:`2dbc8f03` - doc: remove references to 2.0 in intro
- :acrn-commit:`2d61e512` - doc: Release Notes v1.2
- :acrn-commit:`f33886d9` - doc: add new scenario-based intro
- :acrn-commit:`5b3b8efe` - doc: incorporate new scenario-based hardware doc
- :acrn-commit:`0b9257df` - doc: update Getting started guide for Intel NUC software setup
- :acrn-commit:`39aa209d` - doc: update Getting started guide for Intel NUC software setup
- :acrn-commit:`a55436b5` - doc:update Getting started guide for Intel NUC software setup
- :acrn-commit:`664fa27d` - doc: update Using SBL on UP2 Board
- :acrn-commit:`e9e59399` - doc: add limitation for UEFI services
- :acrn-commit:`63e66e65` - doc: hybrid mode scenario introduction
- :acrn-commit:`6b756b8b` - doc: add the SDC2 scenario doc into the navigation
- :acrn-commit:`a3251d85` - doc: incorporate scenario-based doc update
- :acrn-commit:`defac8d1` - doc: apply edits to SDC2 scenario doc
- :acrn-commit:`da744ac3` - doc: adding guide to launch 2 Linux UOSes using SDC2 scenario
- :acrn-commit:`b5140fdd` - doc: update v1.0.1 release notes
- :acrn-commit:`e3349195` - doc: release notes v1.0.1
- :acrn-commit:`4b5a06c1` - doc: improve CSS for home page grid
- :acrn-commit:`f7861687` - doc: fix image proportions on home page for ie
- :acrn-commit:`42d7fbea` - doc: fix broken links after content reorg
- :acrn-commit:`fd2e4391` - doc: additional doc navigation restructuring
- :acrn-commit:`34f9fec4` - doc: simplify navigation with restored doc org
- :acrn-commit:`f88348e9` - doc: continue doc restructuring
- :acrn-commit:`901a65cb` - HV: inject exception for invalid vmcall
- :acrn-commit:`c4f66810` - softirq: disable interrupt when modify timer_list
- :acrn-commit:`f49ab66b` - HV: fix highest severity flag in hybrid mode
- :acrn-commit:`11d4f415` - doc: Reorganize documentation site content
- :acrn-commit:`e188e1f2` - DM USB: xHCI: fix an error in PORTSC emulation
- :acrn-commit:`55a5876e` - DM USB: xHCI: workaround for USB SSD which supports UAS protocol
- :acrn-commit:`0e2cfd2d` - DM USB: add native info in control transfer logging code
- :acrn-commit:`87cafaea` - OVMF release v1.2
- :acrn-commit:`52618d0a` - doc: Update WaaG tutorial launch script and OVMF binary
- :acrn-commit:`8f65bfe6` - README: Fix Getting Started URL
- :acrn-commit:`363daf6a` - HV: return extended info in vCPUID leaf 0x40000001
- :acrn-commit:`accdadce` - HV: Enable vART support by intercepting TSC_ADJUST MSR
- :acrn-commit:`4adc8102` - Makefile: Add install for uefi firmware
- :acrn-commit:`18b4e302` - acrn-config: add README for scenario_config and launch_config
- :acrn-commit:`a03b1341` - acrn-config: generate a patch and apply to acrn-hypervisor
- :acrn-commit:`8adefe26` - acrn-config: generate board information on target board
- :acrn-commit:`107c406b` - dm: array bound checking to avoid buffer overflow
- :acrn-commit:`493ddefd` - dm: fix pointer not checked for null before use
- :acrn-commit:`d4f44bc7` - hv: fix debug message format in 'init_pci_pdev_list'
- :acrn-commit:`bde1d4b1` - acrn-hv: code review fix lib/string.c
- :acrn-commit:`653aa859` - DM: monitor support force stop
- :acrn-commit:`8b27daa7` - tools: acrnctl add '--force' option to 'stop' cmd
- :acrn-commit:`59fd4202` - tools: add force parameter to acrn VM stop operations
- :acrn-commit:`d1c8a514` - doc: Add ovmf option description
- :acrn-commit:`9139f94e` - HV: correct CONFIG_BOARD string of apl up2
- :acrn-commit:`8ee1615e` - doc: fix issues from moving tools to misc/tools
- :acrn-commit:`f44517c7` - final edits 3
- :acrn-commit:`879d0131` - final edit 2
- :acrn-commit:`1ccb9020` - final edits
- :acrn-commit:`d485ed86` - edits
- :acrn-commit:`28e49ac1` - more edits
- :acrn-commit:`cc2714ee` - edits from David review
- :acrn-commit:`7ee02d8e` - Image resize
- :acrn-commit:`87162e8b` - Making three images even smaller
- :acrn-commit:`42960ddc` - Adjust picture size for SGX
- :acrn-commit:`d0f7563d` - Corrected images and formatting
- :acrn-commit:`ce7a126f` - Added 3 SGX images
- :acrn-commit:`01504ecf` - Initial SGX Virt doc upload
- :acrn-commit:`a9c38a5c` - HV:Acrn-hypvervisor Root Directory Clean-up and create misc/ folder for Acrn daemons, services and tools.
- :acrn-commit:`555a03db` - HV: add board specific cpu state table to support Px Cx
- :acrn-commit:`cd3b8ed7` - HV: fix MISRA violation of cpu state table
- :acrn-commit:`a092f400` - HV: make the functions void
- :acrn-commit:`d6bf0605` - HV: remove redundant function calling
- :acrn-commit:`c175141c` - dm: bugfix for remote launch guest issue
- :acrn-commit:`4a27d083` - hv: schedule: schedule to idel after SOS resume form S3
- :acrn-commit:`7b224567` - HV: Remove the mixed usage of inline assembly in wait_sync_change
- :acrn-commit:`baf7d90f` - HV: Refine the usage of monitor/mwait to avoid the possible lockup
- :acrn-commit:`11cf9a4a` - hv: mmu: add hpa2hva_early API for earlt boot
- :acrn-commit:`40475e22` - hv: debug: use printf to debug on early boot
- :acrn-commit:`cc47dbe7` - hv: uart: enable early boot uart
- :acrn-commit:`3945bc4c` - dm: array bound and NULL pointer issue fix
- :acrn-commit:`9fef51ab` - doc: organize release notes into a folder
- :acrn-commit:`ff299d5c` - dm: support VMs communication with virtio-console
- :acrn-commit:`18ecdc12` - hv: uart: make uart base address more readable
- :acrn-commit:`49e60ae1` - hv: refine handler to 'rdpmc' vmexit
- :acrn-commit:`0887eecd` - doc: remove deprecated sos_bootargs
- :acrn-commit:`2e79501e` - doc:udpate using_partition_mode_on_nuc nuc7i7bnh to nuc7i7dnb
- :acrn-commit:`a7b6fc74` - HV: allow write 0 to MSR_IA32_MCG_STATUS
- :acrn-commit:`3cf1daa4` - HV: move vbar info to board specific pci_devices.h
- :acrn-commit:`ce4d71e0` - vpci: fix coding style issue
- :acrn-commit:`a27ce27a` - HV: rename nuc7i7bnh to nuc7i7dnb
- :acrn-commit:`dde20bdb` - HV:refine the handler for 'invept' vmexit
- :acrn-commit:`16a7d252` - DM: ovmf NV storage writeback support
- :acrn-commit:`c787aaa3` - dm: allow High BIOS to be modifiable by the guest
- :acrn-commit:`12955fa8` - hv_main: Remove the continue in vcpu_thread
- :acrn-commit:`f0e1c5e5` - vcpu: init vcpu host stack when reset vcpu
- :acrn-commit:`11e67f1c` - softirq: move softirq from hv_main to interrupt context
- :acrn-commit:`cb9866bc` - softirq:spinlock: correct vioapic/vpic lock usage
- :acrn-commit:`87558b6f` - doc: remove vuart configuration in nuc and up2
- :acrn-commit:`e729b657` - doc: Add ACRN tag or Clear Linux version info for some tutorials
- :acrn-commit:`ffa7f805` - doc: Add tutorial to learn to sign binaries of a Clear Linux image.
- :acrn-commit:`be44e138` - doc: update WaaG doc
- :acrn-commit:`a4abeaf9` - hv: enforce no interrupt to RT VM via vlapic once lapic pt
- :acrn-commit:`97f6097f` - hv: add ops to vlapic structure
- :acrn-commit:`c1b4121e` - dm: virtio-i2c: minor fix
- :acrn-commit:`d28264ff` - doc: update CODEOWNERS for doc reviews
- :acrn-commit:`a90a6a10` - HV: add SDC2 config in hypervisor/arch/x86/Kconfig
- :acrn-commit:`796ac550` - hv: fix symbols not stripped from release binaries
- :acrn-commit:`63e258bd` - efi-stub: update string operation in efi-stub
- :acrn-commit:`05acc8b7` - hv: vuart: bugfix for communication vuart
- :acrn-commit:`ecc472f9` - doc: fix format in WaaG document
- :acrn-commit:`7990f52f` - doc: Add introduction of using Windows Guest OS
- :acrn-commit:`600aa8ea` - HV: change param type of init_pcpu_pre
- :acrn-commit:`e352553e` - hv: atomic: remove atomic load/store and set/clear
- :acrn-commit:`b39526f7` - hv: schedule: vCPU schedule state setting don't need to be atomic
- :acrn-commit:`8af334cb` - hv: vcpu: operation in vcpu_create don't need to be atomic
- :acrn-commit:`540841ac` - hv: vlapic: EOI exit bitmap should set or clear atomically
- :acrn-commit:`0eb08548` - hv: schedule: minor fix about the return type of need_offline
- :acrn-commit:`e69b3dcf` - hv: schedule: remove runqueue_lock in sched_context
- :acrn-commit:`b1dd3e26` - hv: cpu: pcpu_active_bitmap should be set atomically
- :acrn-commit:`1081e100` - hv: schedule: NEED_RESCHEDULE flag should be set atomically
- :acrn-commit:`7d43a93f` - HV: validate multiboot cmdline before merge cmdline
- :acrn-commit:`45afd777` - tools:acrn-crashlog: detect the panic event from all pstore files
- :acrn-commit:`be586b49` - doc:Update Getting started guide for Intel NUC
- :acrn-commit:`009a16bd` - vhostbridge: update vhostbridge to use vdev_ops
- :acrn-commit:`9eba328b` - vdev_ops: add general vdev ops
- :acrn-commit:`37de8f0b` - vbar:msi:msix: export vbar/msi/msix access checking
- :acrn-commit:`c2d25aaf` - pci_vdev: add pci_vdev_ops to pci_vdev
- :acrn-commit:`7a3ea2ad` - DM USB: xHCI: fix corner case of short packet logic
- :acrn-commit:`32d186ba` - DM USB: xHCI: add the resume state for PLS bits
- :acrn-commit:`c3d4cc36` - DM USB: xHCI: refine the logic of Stop Endpoint cmd
- :acrn-commit:`56868982` - DM USB: xHCI: change log for convenience of debugging
- :acrn-commit:`4db7865c` - tools: acrn-manager: fix headers install for ioc cbc tools
- :acrn-commit:`5b1852e4` - HV: add kata support on sdc scenario
- :acrn-commit:`2d4809e3` - hv: fix some potential array overflow risk
- :acrn-commit:`e749ced4` - dm: remove unsafe apis in dm log
- :acrn-commit:`d8b752c4` - dm: fix variable argument list read without ending with va_end
- :acrn-commit:`178c016a` - tools: fix variable argument list read without ending with va_end
- :acrn-commit:`b96a3555` - dm: fix some possible memory leak
- :acrn-commit:`304ae381` - HV: fix "use -- or ++ operations"
- :acrn-commit:`1884bb05` - HV: modify HV RAM and serial config for apl-nuc
- :acrn-commit:`f18dfcf5` - HV: prepare ve820 for apl nuc
- :acrn-commit:`2ec16949` - HV: fix sbuf "Casting operation to a pointer"
- :acrn-commit:`79d03302` - HV: fix vmptable "Casting operation to a pointer"
- :acrn-commit:`9063504b` - HV: ve820 fix "Casting operation to a pointer"
- :acrn-commit:`1aef5290` - doc: Add platform sos info and GUI screenshots against Celadon Guest OS
- :acrn-commit:`714162fb` - HV: fix violations touched type conversion
- :acrn-commit:`5d6c9c33` - hv: vlapic: clear up where needs atomic operation in vLAPIC
- :acrn-commit:`05a4ee80` - hv: cpu: refine secondary cpu start up
- :acrn-commit:`5930e96d` - hv: io_req: refine vhm_req status setting
- :acrn-commit:`1ea3052f` - HV: check security mitigation support for SSBD
- :acrn-commit:`b592404f` - script: set virtio-console BE to stdio for LaaG
- :acrn-commit:`d90fee9f` - hv: add vuart for VM2 in hybrid scenario
- :acrn-commit:`59800214` - DM: Increase hugetlbfs MAX_PATH_LEN from 128 to 256
- :acrn-commit:`44fc5fcb` - doc: fix typos in rtvm workload design doc
- :acrn-commit:`503b71a1` - doc: add guideline for RTVM workload design
- :acrn-commit:`93659f01` - doc: Add introduction of launching Celadon User OS
- :acrn-commit:`4b6dc025` - HV: fix vmptable misc violations
- :acrn-commit:`564a6012` - HV: fix vuart.c "Parameter needs to add const"
- :acrn-commit:`e4d1c321` - hv:fix "no prototype for non-static function"
- :acrn-commit:`4129b72b` - hv: remove unnecessary cancel_event_injection related stuff
- :acrn-commit:`ea849177` - hv: fix interrupt lost when do acrn_handle_pending_request twice
- :acrn-commit:`9a7043e8` - HV: remove instr_emul.c dead code
- :acrn-commit:`254577a6` - makefile: fix parallel build
- :acrn-commit:`3164f397` - hv: Mitigation for CPU MDS vulnerabilities.
- :acrn-commit:`076a30b5` - hv: refine security capability detection function.
- :acrn-commit:`127c98f5` - hv: vioapic: fix interrupt lost and redundant interrupt
- :acrn-commit:`e720dda5` - DM: virtio-i2c: add dsdt info
- :acrn-commit:`b6f9ed39` - DM: virtio-i2c: add msg process logic
- :acrn-commit:`859af9e0` - DM: virtio-i2c: add backend interface
- :acrn-commit:`a450add6` - DM: virtio-i2c: add support for virtio i2c adapter
- :acrn-commit:`2751f137` - dm: remove Execute attribute of usb_pmapper.c
- :acrn-commit:`f3ffce4b` - hv: vmexit: ecx should be checked instead of rcx when xsetbv
- :acrn-commit:`e8371166` - dm: clean up assert in virtio_rnd.c
- :acrn-commit:`842da0ac` - dm: cleanup assert in core.c
- :acrn-commit:`012ec751` - HV: rename vbdf in struct pci_vdev to bdf
- :acrn-commit:`148e7473` - HV: add support for PIO bar emulation
- :acrn-commit:`4be09f24` - HV: enable bar emulation for sos
- :acrn-commit:`af163d57` - HV: add support for 64-bit bar emulation
- :acrn-commit:`09a63560` - hv: vm_manage: minor fix about triple_fault_shutdown_vm
- :acrn-commit:`ebf5c5eb` - hv: cpu: remove CPU up count
- :acrn-commit:`647797ff` - hv: ptdev: refine ptdev active flag
- :acrn-commit:`cb8bbf7b` - dm: clean up the use of errx
- :acrn-commit:`82f7720a` - dm: vhpet: clean up asserts
- :acrn-commit:`aac82750` - dm: vpit: clean up asserts
- :acrn-commit:`81f9837e` - Revert "dm: add "noapic" to rt-linux kernel parameters"
- :acrn-commit:`5a9a7bcd` - dm: gvt: clean up assert
- :acrn-commit:`bd3f2044` - dm: hyper_dmabuf: clean up assert
- :acrn-commit:`56501834` - dm: gc: clean up assert
- :acrn-commit:`4a22801d` - hv: ept: mask EPT leaf entry bit 52 to bit 63 in gpa2hpa
- :acrn-commit:`c64877f5` - tools: add check to verify that running with root privileges
- :acrn-commit:`4c3f298e` - doc:add more description about application constraints
- :acrn-commit:`ae996250` - HV: extract functions from code to improve code reuse and readability
- :acrn-commit:`84e09a22` - HV: remove uint64_t base from struct pci_bar
- :acrn-commit:`5a8703f7` - HV: need to unmap existing EPT mapping for a vbar base (gpa)
- :acrn-commit:`0247c0b9` - Hv: minor cosmetic fix
- :acrn-commit:`f0244b24` - HV: call get_vbar_base() to get the newly set vbar base address in 64-bit
- :acrn-commit:`ed1bdcbb` - HV: add uint64_t bar_base_mapped[PCI_BAR_COUNT] to struct pci_vdev
- :acrn-commit:`65ca6ae4` - HV: add get_vbar_base() to get vbar base address in 64-bit
- :acrn-commit:`7a2f5244` - HV: store the vbar base address in vbar's reg member
- :acrn-commit:`1b4dbdab` - HV: add get_pbar_base() to get pbar base address in 64-bit
- :acrn-commit:`8707834f` - HV: remove the function get_bar_base()
- :acrn-commit:`74b78898` - HV:fix vcpu more than one return entry
- :acrn-commit:`198e0171` - HV:fix vcpu violations
- :acrn-commit:`dc510030` - version: 1.2-unstable

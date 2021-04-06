.. _release_notes_1.4:

ACRN v1.4 (Oct 2019)
####################

We are pleased to announce the release of ACRN version 1.4.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline embedded
development through an open source platform. Check out the :ref:`introduction` for more information.
All project ACRN source code is maintained in the https://github.com/projectacrn/acrn-hypervisor
repository and includes folders for the ACRN hypervisor, the ACRN device
model, tools, and documentation. You can either download this source code as
a zip or tar.gz file (see the `ACRN v1.4 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v1.4>`_)
or use Git clone and checkout commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v1.4

The project's online technical documentation is also tagged to correspond
with a specific release: generated v1.4 documents can be found at https://projectacrn.github.io/1.4/.
Documentation for the latest (master) branch is found at https://projectacrn.github.io/latest/.
ACRN v1.4 requires Clear Linux* OS version 31670.

Version 1.4 Major Features
**************************

What's New in v1.4
==================
* ACRN now conforms to the Microsoft* Hypervisor Top-Level Functional Specification (TLFS).
* ACRN scheduler framework re-architected capabilities have been added.
* WaaG (Windows as a guest) stability and performance has been improved.
* Realtime performance of the RTVM (preempt-RT kernel-based) has been improved.

Document Updates
================
Many new `reference documents <https://projectacrn.github.io>`_ are available, including:

* :ref:`ACRN high-level design <hld>` documents.
* :ref:`enable-s5`
* Enable Secure Boot in the Clear Linux User VM
* :ref:`How-to-enable-secure-boot-for-windows`
* :ref:`asa`

Security Vulnerabilities
************************

We recommend that all developers upgrade to this v1.4 release, which
addresses the following security issues that were discovered in previous releases:

Mitigation for Machine Check Error on Page Size Change
   Improper invalidation for page table updates by a virtual guest operating system for multiple
   Intel |reg| Processors may allow an authenticated user to potentially enable denial of service
   of the host system via local access. Malicious guest kernel could trigger this issue, CVE-2018-12207.

AP Trampoline Is Accessible to the Service VM
   This vulnerability is triggered when validating the memory isolation between the VM and hypervisor.
   The AP Trampoline code exists in the LOW_RAM region in the hypervisor but is
   potentially accessible to the Service VM. This could be used by an attacker to mount DoS
   attacks on the hypervisor if the Service VM is compromised.

Improper Usage Of the ``LIST_FOREACH()`` Macro
   Testing discovered that the MACRO ``LIST_FOREACH()`` was incorrectly used in some cases
   which could induce a "wild pointer" and cause the ACRN Device Model to crash. Attackers
   can potentially use this issue to cause denial of service (DoS) attacks.

Hypervisor Crashed When Fuzzing HC_SET_CALLBACK_VECTOR
   This vulnerability was reported by the Fuzzing tool for the debug version of ACRN. When the software fails
   to validate input properly, an attacker is able to craft the input in a form that is
   not expected by the rest of the application. This can lead to parts of the system
   receiving unintended inputs, which may result in an altered control flow, arbitrary control
   of a resource, or arbitrary code execution.

FILE Pointer Is Not Closed After Using
   This vulnerability was reported by the Fuzzing tool. Leaving the file unclosed will cause a
   leaking file descriptor and may cause unexpected errors in the Device Model program.

Descriptor of Directory Stream Is Referenced After Release
   This vulnerability was reported by the Fuzzing tool. A successful call to ``closedir(DIR *dirp)``
   also closes the underlying file descriptor associated with ``dirp``. Access to the released
   descriptor may point to some arbitrary memory location or cause undefined behavior.

Mutex Is Potentially Kept in a Locked State Forever
   This vulnerability was reported by the Fuzzing tool. Here, ``pthread_mutex_lock/unlock`` pairing was not
   always done. Leaving a mutex in a locked state forever can cause program deadlock,
   depending on the usage scenario.

We recommend that all developers upgrade to ACRN release v1.4.

New Features Details
********************

- :acrn-issue:`3583` - Add Oracle subsystem vendor ID for some virtio devices.
- :acrn-issue:`3600` - remove unused acrn-dm option "pincpu".
- :acrn-issue:`3663` - CPU Sharing: Static Schedule Configuration.
- :acrn-issue:`3813` - CPU Sharing: noop CPU Scheduler.
- :acrn-issue:`3831` - implement performance related TLFS features.
- :acrn-issue:`3832` - Implement Microsoft TLFS minimal requirements.
- :acrn-issue:`3927` - OVMF release V1.4

Fixed Issues Details
********************

- :acrn-issue:`3286` - Remove all TravisCI-related files (including Dockerfiles)
- :acrn-issue:`3329` - Enhance the built-in and online help for `acrnd`
- :acrn-issue:`3330` - Add a check to `acrnd` and `acrnctl` to verify if running with root privileges
- :acrn-issue:`3425` - Guest cannot support more than 4 vcpu
- :acrn-issue:`3429` - [Community][Internal]Add SDC2 scenario to support three post-launched VM for some automotive SDC system
- :acrn-issue:`3491` - [KBL][HV][LaaG]Kill acrn-dm fail after Failed to Launch UOS with 0core
- :acrn-issue:`3503` - [KBL][HV][Hybrid]SOS fail results is different with native when SOS_LTP_syscalls.
- :acrn-issue:`3532` - [UP2][SBL][VBS] sbl-up2 could not boot up when HYBRID kconfig enabled
- :acrn-issue:`3593` - Makefile change which add isd build.
- :acrn-issue:`3598` - [Community-dev][Internal]SEP/SOCWATCH fixes for following coding guidelines
- :acrn-issue:`3609` - [KBLNUC][Stability][RTVM]Host entered standby state with power key flickering in coldboot test.
- :acrn-issue:`3612` - Potential Null pointer be dereferenced in 'usb_dev_request()'
- :acrn-issue:`3622` - [AcrnGT] Kernel PANIC while rebased acrngt patches to mainline kernel v5.2
- :acrn-issue:`3626` - hv: vtd: fix MACRO typos
- :acrn-issue:`3636` - tsc_deadline incorrect issue.
- :acrn-issue:`3644` - HV hang on AC810
- :acrn-issue:`3648` - [REG][KBL/ISD/VBS][HV][SOS]UOS hang when booting UOS with acrnlog running with mem loglevel=6.
- :acrn-issue:`3673` - [Community-dev][Internal]Incorrect reference to OVMF.fd in sample UOS startup script
- :acrn-issue:`3675` - [Community-dev][Internal] cbm length calculation,Extended model judge, print info error
- :acrn-issue:`3708` - [Auto][Daily][OVMF] RTVM can not launch after poweroff, rtvm can not reboot
- :acrn-issue:`3718` - [KBLNUC][Stability][RTVM]WaaG hang after keep WaaG idle in RTVM Create/Destroy test.
- :acrn-issue:`3729` - [KBLNUC]Cannot auto boot 2 VMs with acrnd
- :acrn-issue:`3751` - [acrn-configuration-tool] The default launch script generated by acrn-config for Preemp-RT
  Linux will pass through Ethernet device, which does not match the behavior with devicemodel/samples/nuc/launch_hard_rt.sh
- :acrn-issue:`3754` - [acrn-configuration-tool] WebUI could not select /dev/mmcblk0p1 as UOS rootfs for apl-up2 board, and /dev/mmcblk1p3 for apl-mrb
- :acrn-issue:`3760` - [acrn-configuration-tool]WebUI could not generate configuration patch for a new imported board
- :acrn-issue:`3778` - DM: LIST_FOREACH is improperly used and will result in potential crash
- :acrn-issue:`3787` - crashtool: Invalid pointer validation in "crash_completed_cb()"
- :acrn-issue:`3788` - [acrn-configuration-tool]pci sub class name of NVME contain '-' and ' ' cause wrong macro
- :acrn-issue:`3789` - DM:The return value of snprintf is improperly checked.
- :acrn-issue:`3798` - [acrn-configuration-tool] Failed to make hypervisor by using xml
- :acrn-issue:`3801` - [UP2/KBL][HV][LaaG][Fuzzing]Hypervisor crash when run syz_ic_set_callback_vector.
- :acrn-issue:`3809` - [acrn-configuration-tool]The "uos_type"& items in "passthrough_devices" will disappear when clicking on the drop-down box.
- :acrn-issue:`3811` - [acrn-configuration-tool]Fail to Generate launch_script for a new imported board
- :acrn-issue:`3812` - [acrn-configuration-tool] Generated Launch script is incorrect when select ethernet for apl-mrb with
  Scenario:SDC+Launch Setting: sdc_launch_1uos_laag/aaag
- :acrn-issue:`3817` - DM: FILE Pointer Is Not Closed After Operations in acrn_load_elf
- :acrn-issue:`3821` - DM: DIR handler is referenced after release in npk.c
- :acrn-issue:`3822` - DM: Potential Buffer Overflow due to Unvalidated Input in vm_monitor_blkrescan()
- :acrn-issue:`3825` - DM: 'request_mutex' is potentially not unlocked in tpm_crb.c
- :acrn-issue:`3827` - Service VM power off need at least one minute when passthru NVMe to RTVM
- :acrn-issue:`3834` - [acrn-configuration-tool] UX enhancement: acrn-config end users hope to have interfaces to commit changes to
  local tree or not after setting scenario/launch scripts
- :acrn-issue:`3840` - [Hybrid][UP2][GVT][LaaG]LaaG has no display with uefi boot mode
- :acrn-issue:`3852` - [acrn-configuration-tool]RTVM cannot launched successfully after have launched vxworks or waag.
- :acrn-issue:`3853` - [acrn-configuration-tool] Generated Launch script is incorrect when select audio&audio_codec for nuc7i7dnb with Scenario:SDC
- :acrn-issue:`3859` - VM-Manager: the return value of "strtol" is not validated properly
- :acrn-issue:`3863` - [acrn-configuration-tool]WebUI do not select audio&wifi devices by default for apl-mrb with LaunchSetting: sdc_launch_1uos_aaag
- :acrn-issue:`3879` - [acrn-configuration-tool]The "-k" parameter is unnecessary in launch_uos_id2.sh for RTVM.
- :acrn-issue:`3880` - [acrn-configuration-tool]"--windows \" missing in launch_uos_id1.sh for waag.
- :acrn-issue:`3900` - [WHL][acrn-configuration-tool]Same bdf in generated whl-ipc-i5.xml.
- :acrn-issue:`3913` - [acrn-configuration-tool]WebUI do not give any prompt when generate launch_script for a new imported board
- :acrn-issue:`3914` - [KBL][HV][LaaG] in LaaG, Geekbenck single core result is aroud 83% percent of native
- :acrn-issue:`3917` - [acrn-configuration-tool]Can not select "Network controller" device in wifi&ethernet combobox for Board=nuc6cayh&nuc7i7dnb on WebUI
- :acrn-issue:`3925` - Statically allocate 2 pCPUs for hard RT VM by default
- :acrn-issue:`3932` - [KBL][acrn-configuration-tool]Generated Launch script miss "-m $mem_size" after select passthru_device
- :acrn-issue:`3933` - [KBLNUC][Function][LaaG] can't bootup LaaG
- :acrn-issue:`3937` - [KBL][acrn-configuration-tool][WaaG]Generated Launch script miss boot_audio_option's right_double_quotation_marks
  for Board info:nuc7i7dnb + Launch Setting: industry_launch_1uos_waag
- :acrn-issue:`3947` - [ISD][Stability][WaaG] WaaG auto boot failed after systemctl enable acrnd during S5 testing
- :acrn-issue:`3948` - [KBL][acrn-configuration-tool]'keep_gsi' should not set along with waag vm
- :acrn-issue:`3949` - [KBL][acrn-configuration-tool]'virtio-blk' was generated in launch script of Preempt-RT, should remove it
- :acrn-issue:`3974` - [WHL][Function][RT_LaaG][REG]RTVM cannot poweroff & acrnctl stop.
- :acrn-issue:`3975` - [WHL][Function][RT_LaaG][REG]RTVM cannot poweroff & acrnctl stop.
- :acrn-issue:`3993` - trampoline code in hypervisor potentially be accessible to service VM

Known Issues
************

- :acrn-issue:`3979` - The boot uos script will display the sos information on the uos screen for about 2s
- :acrn-issue:`4005` - Fail to create WaaG image using ISO only on WHL
- :acrn-issue:`4042` - RTVM UOS result is invalid when run cpu2017 with 3 and 1 core.
- :acrn-issue:`4043` - Windows guest can not get normal IP after passthru Ethernet
- :acrn-issue:`4045` - Adding USB mediator in launch script, it takes a long time to start windows, about 13 minutes.
- :acrn-issue:`4046` - Error info pop up when run 3DMARK11 on Waag
- :acrn-issue:`4047` - passthru usb, when WaaG boot at "windows boot manager" menu, the usb keyboard does not work.
- :acrn-issue:`4048` - Scaling the media player while playing a video, then the video playback is not smooth
- :acrn-issue:`4049` - Only slot-2 can work in "-s n,passthru,02/00/0 \" for RTVM, other slots are not functional

Change Log
**********

These commits have been added to the acrn-hypervisor repo since the v1.3
release in Sep 2019 (click on the CommitID link to see details):

.. comment

   This list is obtained from this git command (update the date to pick up
   changes since the last release):

   git log --pretty=format:'- :acrn-commit:`%h` - %s' --after="2019-09-28"

- :acrn-commit:`e0d14b70` - Doc: Grammatical edits to the 1.4 Release Notes.
- :acrn-commit:`d8bd5088` - doc: Release notes v1.4
- :acrn-commit:`90a61134` - Doc: Grammatical edits to the Advisory doc.
- :acrn-commit:`c6bccd5c` - doc: Add Advisory notes
- :acrn-commit:`94394ae9` - Doc: Grammatical edits to the Enable S5 Guide.
- :acrn-commit:`b16e5987` - doc: enable s5 guide
- :acrn-commit:`d473cafe` - dm: Add licenses to the scripts.
- :acrn-commit:`79294b39` - Doc: Final edits for the HLD-Security doc.
- :acrn-commit:`865d1a22` - doc: add hld-security guest secure boot description
- :acrn-commit:`a4713fce` - doc: add copyright/license header to doc scripts
- :acrn-commit:`04767070` - Doc: Final edits to the HV Hypercall doc.
- :acrn-commit:`dcfa7587` - Doc: Final edits to the CPU Virt doc
- :acrn-commit:`ce46f35d` - doc: review edits for rt_industry doc
- :acrn-commit:`3298891f` - Doc: Final edits to the HLD Overview doc.
- :acrn-commit:`a74a7551` - Doc: Final edits to Memory Mangt HLD doc.
- :acrn-commit:`1c3f16f5` - doc: review edit for enable_laag_secure_boot
- :acrn-commit:`ae126bd5` - doc: review edits for acrn_configuration_tool
- :acrn-commit:`9687d72e` - doc: add cores and threads for CPU of supported hardware
- :acrn-commit:`2d0739bf` - doc: fix error in building_from_source doc
- :acrn-commit:`3b977eef` - doc: clean up the docs in try using acrn table.
- :acrn-commit:`2a3178aa` - doc: Update Using Windows as Guest VM on ACRN
- :acrn-commit:`9bd274ae` - doc:modify ubuntu build on 18.04
- :acrn-commit:`7d818c82` - doc: Stop using kconfig to make a customized efi.
- :acrn-commit:`67c64522` - dm: fix memory free issue for xhci
- :acrn-commit:`3fb1021d` - Doc: Minor grammatical edits on various files.
- :acrn-commit:`72f71192` - doc: fix doc build errors previously masked
- :acrn-commit:`2a6f2fa8` - hv: update virtual interrupts HLD
- :acrn-commit:`3314857a` - Doc: update conf.py file to include v1.4
- :acrn-commit:`b20a67f8` - doc: clean up waag-secure-boot doc
- :acrn-commit:`9d01d8ad` - doc: instruction of enabling the laag secure boot
- :acrn-commit:`af61b486` - doc: update timer hld
- :acrn-commit:`ea601e42` - doc: update memory management hld
- :acrn-commit:`c8abc7cb` - Added entry for waag-secure-boot tutorial (added in #3883)
- :acrn-commit:`f34f87fa` - doc:update acrn_configuration_tool
- :acrn-commit:`0e652546` - doc: add waag secure boot enabling
- :acrn-commit:`0d2cdd95` - Clean up language in the acrn_quick_setup script.
- :acrn-commit:`cc61ae7c` - doc: schedule_vcpu was removed
- :acrn-commit:`fbc54a18` - doc: Remove apl gsg and merge contents into rt gsg
- :acrn-commit:`f7651009` - doc: remove redundant copy of ovmf.fd firmware
- :acrn-commit:`67d4a38f` - doc: remove the guest cpu number option '-c' from the document
- :acrn-commit:`fe73b2df` - doc: OVMF high level description
- :acrn-commit:`625a6aab` - Doc: Update system power management doc
- :acrn-commit:`383d53b4` - doc: Update hv power management doc
- :acrn-commit:`4586acd4` - document: update HLD for hypervisor overview
- :acrn-commit:`5583c864` - document: update HLD for cpu virtualization
- :acrn-commit:`2077d79d` - doc: fix remaining masked doc build errors
- :acrn-commit:`f3f638fa` - doc: doc build errors not being reported
- :acrn-commit:`9e34a3fd` - Doc: remove broken links in RNs.
- :acrn-commit:`97a0464a` - doc: fix broken include paths
- :acrn-commit:`1a277f75` - doc: fix vuart-virt-hld errors
- :acrn-commit:`bbc228e6` - doc: Add industry argument for auto setup script
- :acrn-commit:`fd821d55` - doc: add atkbdc emulation doc
- :acrn-commit:`6513db40` - doc: add HLD for CAT
- :acrn-commit:`b6007d05` - document: update HLD for hypervisor startup
- :acrn-commit:`93c45f1a` - Doc: update physical interrupt HLD
- :acrn-commit:`f3f48c3b` - doc: add system timer virtualization
- :acrn-commit:`924f4007` - doc: Adding a section for ACRN requirements on a processor
- :acrn-commit:`586a947d` - doc: add hostbridge emulation doc
- :acrn-commit:`2e2c3db5` - doc: add RTC emulation in hypervisor doc
- :acrn-commit:`b5491474` - doc: add virtio-gpio doc
- :acrn-commit:`5466c8e4` - Doc: remove tutorials/rt_linux.rst file
- :acrn-commit:`d0e1f05e` - doc: Align the updates of rt gsg with 1.3
- :acrn-commit:`b4a4d46c` - doc: add entry for passthru realization
- :acrn-commit:`e85ff56c` - doc: modify virtio-i2c doc path
- :acrn-commit:`abfe3e40` - doc: add UART emulation in hypervisor doc
- :acrn-commit:`9493fcdf` - doc: add ahci virtualization introduction
- :acrn-commit:`2cfcb62d` - document: update HLD for vm management
- :acrn-commit:`b92cb4cb` - acrn-config: chose ttyS1 for vuart1
- :acrn-commit:`95a9f6d9` - hv: update the flow to get trampoline buffer in direct boot mode
- :acrn-commit:`c09723bd` - hv[v3]: hide AP trampoline code from service VM
- :acrn-commit:`04f07535` - hv:refine modify_or_del_pte/pde/pdpte()function
- :acrn-commit:`3f3a51ba` - Revert "Makefile: add default defconfig for new board"
- :acrn-commit:`99e2d6bc` - Revert "OVMF release v1.4"
- :acrn-commit:`c1225050` - acrn-config: add 'xhci' usb mediator for laag and waag
- :acrn-commit:`c0e1a5d7` - acrn-config: add serial config in new $(board).config
- :acrn-commit:`9ddf2766` - Makefile: add default defconfig for new board
- :acrn-commit:`382af0b1` - acrn-config: refine mem_size_set function
- :acrn-commit:`1818dfd9` - acrn-config: refine interrupt_storm function
- :acrn-commit:`958830fb` - acrn-config: add support to generate launch script
- :acrn-commit:`70a405b8` - acrn-config: remove runC script from unnecessary launch script
- :acrn-commit:`2e647844` - acrn-config: add config files for whl-ipc-i7 board
- :acrn-commit:`7587ccba` - acrn-config: add config files for whl-ipc-i5 board
- :acrn-commit:`084bf6e1` - acrn-config: remove parser for console
- :acrn-commit:`a503fdce` - HV: Fix poweroff issue of hard RTVM
- :acrn-commit:`5ca26d3b` - Modify KBL-NUC/SDC for default build remove acrn.efi and modify KBL-NUC/SDC for default build Tracked-On: #3953 Signed-off-by: wenlingz <wenling.zhang@intel.com>
- :acrn-commit:`c94b1fcd` - acrn-config: 'keep_gsi' flag set for Android vm
- :acrn-commit:`96f4d511` - acrn-config: Remove virtio-blk for PREEMPT-RT LINUX
- :acrn-commit:`cc7a85ae` - acrn-config: modify vxworks uos id for industry launch config
- :acrn-commit:`39f300a5` - acrn-config: parse rootfs_img and refine virtio-blk
- :acrn-commit:`5f5f3dfd` - acrn-config: modify board name to uos name
- :acrn-commit:`5cbc97ba` - acrn-config: add mem_size for launch vm
- :acrn-commit:`79fb22de` - acrn-config: add the '"' character for launch script
- :acrn-commit:`d5c3523d` - hv: Update industry scenarios configuration
- :acrn-commit:`6f7081f6` - acrn-config: remove vm3 for industry scenario
- :acrn-commit:`9143e563` - dm: update ACPI with latest ASL standard
- :acrn-commit:`5f8e7a6c` - hv: sched: add kick_thread to support notification
- :acrn-commit:`810305be` - hv: sched: disable interrupt when grab schedule spinlock
- :acrn-commit:`15c6a3e3` - hv: sched: remove do_switch
- :acrn-commit:`f04c4912` - hv: sched: decouple scheduler from schedule framework
- :acrn-commit:`cad195c0` - hv: sched: add pcpu_id in sched_control
- :acrn-commit:`84e5a8e8` - OVMF release v1.4
- :acrn-commit:`feba8369` - acrn-config: refine ttyS info of board file
- :acrn-commit:`b6a80520` - acrn-config: filter out the proper wifi/ethernet device
- :acrn-commit:`defeb851` - acrn-config: fix the issue no error message in launch setting
- :acrn-commit:`d9f0d8dc` - acrn-config: fix the wrong 'key' type returned to webUI
- :acrn-commit:`e7134585` - makefile: add dash support to build efi
- :acrn-commit:`9ea7a85c` - acrn-config: set default package value for _S3 and _S5
- :acrn-commit:`24d3eaba` - acrn-config: skip git environment check when not do git commit
- :acrn-commit:`fbd8597f` - acrn-config: refine 'lpc' setting with console type
- :acrn-commit:`2e62ad95` - hv[v2]: remove registration of default port IO and MMIO handlers
- :acrn-commit:`73b8c91e` - Misc: lifemngr-daemon-on-UOS for windows
- :acrn-commit:`82a0d39e` - hv:fix reference to uninitialized variable in vmsi_remap()
- :acrn-commit:`1c7bf9fd` - acrn-config: refine the vbootloader of vm
- :acrn-commit:`a7162359` - acrn-config: add '--windows' option for WaaG vm
- :acrn-commit:`343aabca` - doc:Update hypercall and upcall
- :acrn-commit:`6f9367a5` - Doc: Add ART virtualization hld
- :acrn-commit:`b3142e16` - doc: update hld-security verified boot section
- :acrn-commit:`edffde4e` - doc: update MSR virtualization in HLD
- :acrn-commit:`227ee64b` - doc: update IO/MMIO HLD
- :acrn-commit:`d541ee90` - doc: update CR HLD
- :acrn-commit:`050c0880` - doc: update CPUID HLD
- :acrn-commit:`d81872ba` - hv:Change the function parameter for init_ept_mem_ops
- :acrn-commit:`0f70a5ca` - hv: sched: decouple idle stuff from schedule module
- :acrn-commit:`27163df9` - hv: sched: add sleep/wake for thread object
- :acrn-commit:`9b8c6e6a` - hv: sched: add status for thread_object
- :acrn-commit:`fafd5cf0` - hv: sched: move schedule initialization to each pcpu init
- :acrn-commit:`dadcdcef` - hv: sched: support vcpu context switch on one pcpu
- :acrn-commit:`7e66c0d4` - hv: sched: use get_running_vcpu to replace per_cpu vcpu with cpu sharing
- :acrn-commit:`891e4645` - hv: sched: move pcpu_id from acrn_vcpu to thread_object
- :acrn-commit:`f85106d1` - hv: Do not reset vcpu thread's stack when reset_vcpu
- :acrn-commit:`3072b6fc` - Doc: Grammar add for config tool doc
- :acrn-commit:`6f5dd2da` - doc: acrn_configuration_tool add one more scenario xml element description
- :acrn-commit:`1d194ede` - hv: support reference time enlightenment
- :acrn-commit:`048155d3` - hv: support minimum set of TLFS
- :acrn-commit:`009d835b` - acrn-config: modify board info of block device info
- :acrn-commit:`96dede43` - acrn-config: modify ipu/ipu_i2c device launch config of apl-up2
- :acrn-commit:`001c929d` - acrn-config: correct launch config info for audio/wifi device of apl-mrb
- :acrn-commit:`2a647fa1` - acrn-config: define vm name for Preempt-RT Linux in launch script
- :acrn-commit:`a2430f13` - acrn-config: refine board name with undline_name api
- :acrn-commit:`95b9ba36` - acrn-config: acrn-config: add white list to skip item check
- :acrn-commit:`fc40ee4c` - vm-manager: fix improper return value check for "strtol()"
- :acrn-commit:`9c67d9b9` - grammar edits for the hld security document
- :acrn-commit:`15e8130f` - doc: hld-security hypervisor enhancement section update
- :acrn-commit:`27272634` - doc: hld-security memory management enhancement update
- :acrn-commit:`81a76662` - doc: hld-security introduction update
- :acrn-commit:`38d70690` - doc: add description for usb-virt-hld
- :acrn-commit:`8a2a56e8` - Doc: Update hld-trace-log.rst
- :acrn-commit:`96b4a6db` - acrn-config: add 'boot_audio_option' while auido/audio_codec set
- :acrn-commit:`1326eec4` - acrn-config: refine the tools for audio/audio_codec
- :acrn-commit:`950e3aa2` - acrn-config: refine parameters for media_pt function
- :acrn-commit:`292d1a15` - hv:Wrap some APIs related with guest pm
- :acrn-commit:`988c1e48` - doc: Align CL version for RT GSG and NUC GSG
- :acrn-commit:`e7ef57a9` - dm: fix mutex lock issue in tpm_rbc.c
- :acrn-commit:`73ac285e` - acrn-config: add 'run_container' back to the launch script
- :acrn-commit:`55e4f0af` - acrn-config: remove '-V' option from launch config
- :acrn-commit:`aee3bc36` - acrn-config: enable item check for launch config tool
- :acrn-commit:`98dc755e` - dm: NVME bdf info update on KBLNUC7i7DNH
- :acrn-commit:`712dfa95` - minor content edits to virtio-rnd doc
- :acrn-commit:`89ec29e1` - doc: merge random device doc to virtio-rnd doc
- :acrn-commit:`f2fb227b` - doc: detail change for hld-devicemodel
- :acrn-commit:`d204fdee` - doc: add 'rsync' to the ACRN builder container
- :acrn-commit:`d8deaa4b` - dm: close filepointer before exiting acrn_load_elf()
- :acrn-commit:`b5f77c07` - doc: add socket console backend for virtio-console
- :acrn-commit:`d3ac30c6` - hv: modify SOS i915 plane setting for hybrid scenario
- :acrn-commit:`c74a197c` - acrn-config: modify SOS i915 plane setting for hybrid xmls
- :acrn-commit:`e1a2ed17` - hv: fix a bug that tpr threshold is not updated
- :acrn-commit:`afb3608b` - acrn-config: add confirmation for commit of generated source in config app
- :acrn-commit:`8eaee3b0` - acrn-config: add "enable_commit" parameter for config tool
- :acrn-commit:`780a53a1` - tools: acrn-crashlog: refine crash complete code
- :acrn-commit:`43b2327e` - dm: validation for input to public functions
- :acrn-commit:`477f8331` - dm: modify DIR handler reference position
- :acrn-commit:`de157ab9` - hv: sched: remove runqueue from current schedule logic
- :acrn-commit:`837e4d87` - hv: sched: rename schedule related structs and vars
- :acrn-commit:`89f53a40` - acrn-config: supply optional passthrough device for vm
- :acrn-commit:`82609463` - doc: Clear Linux "ACRN builder" container image
- :acrn-commit:`44d2a56b` - doc: fix missing words issue in acrn configuration tool doc
- :acrn-commit:`d19592a3` - hv: vmsr: disable prmrr related msrs in vm
- :acrn-commit:`de0a5a48` - hv:remove some unnecessary includes
- :acrn-commit:`72232daa` - dm: reduce potential crash caused by LIST_FOREACH
- :acrn-commit:`e6e0e277` - dm: refine the check of return value of snprintf
- :acrn-commit:`44c11ce6` - acrn-config: fix the issue some select boxes disappear after edited
- :acrn-commit:`c7ecdf47` - Corrected number issue in GSG for ACRN Ind Scenario file
- :acrn-commit:`051a8e4a` - doc: update Oracle driver install
- :acrn-commit:`b73b0fc2` - doc: ioc: remove two unused parts
- :acrn-commit:`6f7ba36e` - doc: move the "Building ACRN in Docker" user guide
- :acrn-commit:`1794d994` - doc: update doc generation tooling to only work within the $BUILDDIR
- :acrn-commit:`0dac373d` - hv: vpci: remove pci_msi_cap in pci_pdev
- :acrn-commit:`b1e43b44` - hv: fix error debug message in hcall_set_callback_vector
- :acrn-commit:`62ed91d3` - acrn-config: update vcpu affinity in web UI
- :acrn-commit:`c442f3f4` - acrn-config: keep align with vcpu_affinity for vm config
- :acrn-commit:`db909edd` - acrn-config: refine the data type for member of class
- :acrn-commit:`ee66a94c` - acrn-config: grab Processor CPU number from board information
- :acrn-commit:`fcbf9d7b` - makefile: fix efi stub install issue
- :acrn-commit:`c3eb0d7f` - dm: switch to launch RT_LaaG with OVMF by default
- :acrn-commit:`8c9c8876` - hv: vpci: remove PC-Card type support
- :acrn-commit:`a4d562da` - dm: Add Oracle subsystem vendor ID
- :acrn-commit:`bb1a8eea` - acrn-config: fix pci sub class name contain "-" and ' '
- :acrn-commit:`43410fd0` - Makefile: Add new build target for apl-up2/uefi/hybrid
- :acrn-commit:`d0489ef3` - Makefile: Add acrn build/install functions
- :acrn-commit:`df5ef925` - Misc: life_mngr clear compile warning
- :acrn-commit:`91366b87` - Misc: lifemngr add Makefile
- :acrn-commit:`28b50463` - hv: vm: properly reset pCPUs with LAPIC PT enabled during VM shutdown/reset
- :acrn-commit:`0906b25c` - Makefile: build default acrn.efi with nuc6cayh
- :acrn-commit:`64742be8` - doc: fix broken link in release notes 1.3
- :acrn-commit:`9b1caeef` - version: 1.4-unstable

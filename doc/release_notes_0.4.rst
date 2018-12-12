.. _release_notes_0.4:

ACRN v0.4 (Dec 2018)
####################

We are pleased to announce the release of Project ACRN version 0.4.

ACRN is a flexible, lightweight reference hypervisor, built with
real-time and safety-criticality in mind, optimized to streamline
embedded development through an open source platform. Check out the
:ref:`introduction` for more information.


All project ACRN source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, and documentation.
You can either download this source code as a zip or tar.gz file (see
the `ACRN v0.4 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v0.4>`_ or
use Git clone and checkout commands:

.. code-block:: bash

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v0.4

The project's online technical documentation is also tagged to correspond
with a specific release: generated v0.4 documents can be found at
https://projectacrn.github.io/0.4/.  Documentation for the latest
(master) branch is found at https://projectacrn.github.io/latest/.


Version 0.4 new features
************************

- :acrn-issue:`1824` - implement "wbinvd" emulation 
- :acrn-issue:`1859` - Doc: update GSG guide to avoid issue "black screen"
- :acrn-issue:`1878` - The "Using Ubuntu as the Service OS" tutorial is outdated and needs to be refreshed
- :acrn-issue:`1926` - `kernel-doc` causing `make doc` failure (because of upcoming Perl changes)
- :acrn-issue:`1927` - Simplify the `launch_uos.sh` script by pointing at the latest iot-lts2018 kernel by default

Fixed Issues
************

- :acrn-issue:`677` - SSD Disk ID is not consistent between SOS/UOS
- :acrn-issue:`1777` - After UOS plays video for several minutes, the UOS image will be stagnant
- :acrn-issue:`1778` - MSDK: 1080p H264 video decode fails in UOS
- :acrn-issue:`1779` - gfxbench cannot run in SOS&UOS
- :acrn-issue:`1781` - Can not recognize the SD card in the SOS
- :acrn-issue:`1792` - System hang and reboot after run "LaaG Forced GPU Reset: subtest error-state-capture-vebox" in UOS
- :acrn-issue:`1794` - After SOS boots up, there's no output on SOS screen
- :acrn-issue:`1795` - SOS fails to get IP address
- :acrn-issue:`1825` - Need to clear memory region used by UOS before it exit
- :acrn-issue:`1837` - 'acrnctl list' shows incomplete VM names

Known Issues
************

:acrn-issue:`1319` - SD card pass-through: UOS can't see SD card after UOS reboot.
   SD card could not be found after UOS reboot in pass-through mode.

   **Impact:** There is no SD card after UOS reboot.

   **Workaround:** None. The issue will be fixed in the next release.

:acrn-issue:`1773` - USB Mediator: Can't find all devices when multiple usb devices connected[Reproduce rate:60%]
   After booting UOS with multiple USB devices plugged in, there's a 60% chance that
   one or more devices are not discovered.

   **Impact:** Cannot use multiple USB devices at same time.

   **Workaround:** Unplug and plug-in the unrecognized device after booting.

:acrn-issue:`1774` - UOS can't stop by command: acrnctl stop [vm name] in SOS
   After launching UOS in SOS by "acrnctl start" command, UOS VM failed
   to be stopped by "acrnctl stop" command.

   **Impact:** Can't stop UOS in SOS.

   **Workaround:** None. The issue will be fixed in the next release.

:acrn-issue:`1775` - [APL UP2]ACRN debugging tool - acrntrace cannot be used in SOS
   There are no acrntrace devices "acrn_trace*" under SOS /dev.

   **Impact:** acrntrace cannot be used in SOS.

   **Workaround:** None. The issue will be fixed in the next release.

:acrn-issue:`1776` - [APL UP2]ACRN debugging tool - acrnlog cannot be used in SOS
   There are no acrnlog devices "acrn_hvlog*" under SOS /dev.

   **Impact:** acrnlog cannot be used in SOS.

   **Workaround:** None. The issue will be fixed in the next release.

:acrn-issue:`1780` - Some video formats cannot be played in SOS
   Video files with these encodings are not supported in the SOS:
   H265_10bits, VP8, VP9, VP9_10bits, H265.720p.

   **Impact:** Cannot play those formats of videos in SOS.

   **Workaround:** None. The issues will be fixed in the next release.

:acrn-issue:`1782` - UOS failed to get IP address with the pass-through network card
   After a network card is pass-through to UOS, it fails to get an IP address in UOS.

   **Impact:** Cannot use network in UOS.

   **Workaround:** None. The issues will be fixed in the next release.

:acrn-issue:`1796` - APL NUC fails to reboot sometimes
   After APL NUC boots to SOS, the "reboot" command sometimes fails to reboot the SOS.

   **Impact:** Cannot reboot SOS.

   **Workaround:** Power off and boot again. The issues will be fixed in the next release.

:acrn-issue:`1986` - UOS will hang once watchdog reset triggered
   If Launching UOS with “-s 8,wdt-i6300esb”, UOS will hang if the watchdog reset is triggered.

   **Impact:** UOS cannot self-recover after a watchdog reset is triggered.

   **Workaround:** None.

:acrn-issue:`1987` - UOS will have same MAC address after launching UOS with virio-net
   After launching UOS with virio-net, UOS on different devices have the same MAC address.

   **Impact:** A UOS network conflict will exist.

   **Workaround:** None. This issues will be fixed in next release.

:acrn-issue:`1991` - Input is useless in UART Console for corner case
   Input is useless in UART Console for a corner case,
   demonstrated with these steps:

   1) Boot to SOS

   2) ssh into the SOS.

   3) use "./launch_UOS.sh" to boot UOS.

   4) On the host, use "minicom -s dev/ttyUSB0".

   5) Use "sos_console 0" to launch SOS.

   **Impact:** Failed to use UART for input in corner case.

   **Workaround:** Enter other keys before typing :kbd:`Enter`.
 
:acrn-issue:`1996` - There is an error log when using "acrnd&" to boot UOS
   An error log is printed when starting acrnd as a background job
   (``acrnd&``) to boot UOS. The UOS still boots up
   normally, but prints: “Failed to open the socket(sos-lcs) to query the reason for the wake-up.
   Activating all vms when acrnd & to boot uos."

   **Impact:** UOS boots normally, but prints an error log message.

   **Workaround:** None.

:acrn-issue:`2000` - After launching UOS with Audio pass-through, Device (I2C0) doesn't exist in UOS DSDT.dsl
   After launching UOS with Audio pass-through, Device (I2C0) doesn't exist in UOS DSDT.dsl

   **Impact:** Cannot use Audio device

   **Workaround:** None.

:acrn-issue:`2030` - UP2 fails to boot with uart=disabled for hypervisor
   SOS boots up fail following GSG document guide.

   **Impact:** SOS boots up fail on APL UP2

   **Workaround:** A step-by-step workaround has been updated in Github issue.

:acrn-issue:`2031` - UP2 serial port has no output with uart=mmio@0x91622000 for hypervisor
   After SOS starts, there's no display on the screen. Though ssh connection is successful, the serial port has no output.

   **Impact:** UP2 serial port has no output

   **Workaround:** A step-by-step workaround has been updated in Github issue.

.. comment
   Use the syntax:

   :acrn-issue:`663` - Short issue description
     Longer description that helps explain the problem from the user's
     point of view (not internal reasons).  **Impact:** What's the
     consequences of the issue, and how it can affect the user or system.
     **Workaround:** Describe a workaround if one exists (or refer them to the
     :acrn-issue:`663`` if described well there. If no workaround, say
     "none".


Change Log
**********

These commits have been added to the acrn-hypervisor repo since the v0.3
release in Nov 2018 (click on the CommitID link to see details):

.. comment

   This list is obtained from the command:
   git log --pretty=format:'- :acrn-commit:`%h` %s' --after="2018-03-01"

- :acrn-commit:`7ee0e2e2` tools: acrnctl: Fix path error when run "acrnctl add" cmd
- :acrn-commit:`9761eede` hv: cleanup IA32_PAT emulation code r.w.t. to the refactored guest_msrs[]
- :acrn-commit:`b6aaf1b8` hv: MSRs may need isolation between normal and secure world
- :acrn-commit:`92bbb545` hv: rearrange data structure for emulated MSRs
- :acrn-commit:`7fce2462` dm: apply new mevent API to avoid race issue in mei
- :acrn-commit:`64d9c59a` dm: enhence the mevent API
- :acrn-commit:`eec3a342` dm: fix the race issue in mevent_del
- :acrn-commit:`87e7bdb9` DM: updating launch_uos.sh
- :acrn-commit:`9e0562f4` hv: add obvious comment for empty else clause following else if
- :acrn-commit:`d36b44f2` hv: avoid to use ``++`` or ``--`` operators in an expression
- :acrn-commit:`f33edc67` hv: fix reference parameter to procedure is reassigned
- :acrn-commit:`36be890e` hv: fix included file not protected with #define
- :acrn-commit:`ae9d4361` hv: minimize the case of "identifier reuse"
- :acrn-commit:`3afc5113` hv: acpi: remove weak parse_madt
- :acrn-commit:`c616a422` hv: fix string assigned to non const object
- :acrn-commit:`c3799146` hv: remove "i915.enable_initial_modeset"
- :acrn-commit:`c3c93202` hv: fix "Array has no bounds specified" in vmsr.c
- :acrn-commit:`01cb6ba8` hv: fix one MISRA-C violation in mtrr.c
- :acrn-commit:`fe1ace4f` doc: fix doc misspellings
- :acrn-commit:`fa99dba3` Update doc/getting-started/apl-nuc.rst
- :acrn-commit:`f657f401` doc:  update gsg to adapt latest release
- :acrn-commit:`e24039a7` doc: tweak CSS for doxygen API usability
- :acrn-commit:`3ca64c5b` dm: add "break" removed by mistake.
- :acrn-commit:`908acb50` hv: add 'no-omit-frame-pointer' in debug version
- :acrn-commit:`9bb16bce` hv: fix type conversion without cast with explicit conversion
- :acrn-commit:`79463fd5` hv: avoid using of mixed mode arithmetic
- :acrn-commit:`9c133c7b` hv: lib: refine print_decimal
- :acrn-commit:`7a62154e` hv: remove the theoretic infinite loop
- :acrn-commit:`5d19962d` security: remove cflag _FORTIFY_SOURCE in hypervisor
- :acrn-commit:`d737d6e6` tools: acrnlog: give user hint when acrn hvlog devices not found
- :acrn-commit:`d85a0b70` tools: acrntrace: give user hint when acrn trace devices not found
- :acrn-commit:`9ea93ce6` hv: x2APICv support on platforms without support for APICv reg virtualization
- :acrn-commit:`9d4b5d7e` DM USB: add some preparing time for xHCI emulation before resuming.
- :acrn-commit:`b159d66f` DM USB: refine the polling thread for libusb events
- :acrn-commit:`966c5872` DM USB: xHCI: fix potential NULL pointer issue.
- :acrn-commit:`5b39fd0e` DM USB: xHCI: fix error logic of allocating xHCI slot
- :acrn-commit:`32c4ce9b` DM USB: xHCI: refine the xHCI S3 process
- :acrn-commit:`9e471d72` DM USB: xHCI: refine the PLC bit emulation logic during S3
- :acrn-commit:`29e81501` DM USB: xHCI: refine error handling logic for ctrl transfer
- :acrn-commit:`f73cf211` hv: fix 'Unused procedure parameter'
- :acrn-commit:`b261e74d` dm: virtio poll mode support for RT
- :acrn-commit:`7cc8566d` hv: fixes related to unused API and uninitialized variable
- :acrn-commit:`f0d3f1c9` HV: Remove some comments for crypto library
- :acrn-commit:`d7232ebb` hv: trusty: refine struct trusty_mem
- :acrn-commit:`5fd6021d` doc: hv: add comments to timer APIs for documentation
- :acrn-commit:`2dbb0cba` doc: fix citation references in modularity doc
- :acrn-commit:`e2a8989f` doc: add a document on considerations and current status of hypervisor modularization
- :acrn-commit:`3b54dd2a` doc: add some "sudo" for code
- :acrn-commit:`945fdd8a` doc: update the directory to "~/"
- :acrn-commit:`0ff74b13` doc: delete "install build tool" about
- :acrn-commit:`1a959d0f` doc: Update note for the directory of UOS image
- :acrn-commit:`e2e9a3e9` doc: Add the note for the directory of UOS image
- :acrn-commit:`10522423` doc: add note for the directory of UOS image
- :acrn-commit:`099c605e` doc: Modify to "/boot/efi"
- :acrn-commit:`ceed3106` Update using_ubuntu_as_sos.rst
- :acrn-commit:`b1db77eb` doc: Update the grub part and add code for NVMe
- :acrn-commit:`4b2e7f11` Delete AGL about
- :acrn-commit:`be70145f` Delete AGL about
- :acrn-commit:`96a2946d` Delete AGL about
- :acrn-commit:`6c8c46af` delete AGL about
- :acrn-commit:`ce89d26e` Delete using_AGL_as_uos.rst
- :acrn-commit:`3d96e356` Rename using_AGL_as_uos to using_AGL_as_uos.rst
- :acrn-commit:`90c27157` Create using AGL as UOS
- :acrn-commit:`2bc24f87` Upload the images for "using_AGL_as_uos"
- :acrn-commit:`12e66b98` Update using_ubuntu_as_sos.rst
- :acrn-commit:`ecff0bf9` Update the layout of packages
- :acrn-commit:`50f17832` Update using_ubuntu_as_sos.rst
- :acrn-commit:`1afb0f13` Update using_ubuntu_as_sos.rst
- :acrn-commit:`06b2ab55` Update using_ubuntu_as_sos.rst
- :acrn-commit:`e4941b22` Update using_ubuntu_as_sos.rst
- :acrn-commit:`65f21a77` Update the version of Ubuntu to 18.04
- :acrn-commit:`abfa1c16` update the length of ＊
- :acrn-commit:`1664ba5f` Update using_ubuntu_as_sos.rst
- :acrn-commit:`f3527c63` Update using_ubuntu_as_sos.rst
- :acrn-commit:`e4b616d5` Update using_ubuntu_as_sos.rst
- :acrn-commit:`ab005bc8` Update using_ubuntu_as_sos.rst
- :acrn-commit:`2d685a13` Update with Clear Linux 26440
- :acrn-commit:`b38629b8` hv: fix 'Space missing before or after binary operator'
- :acrn-commit:`e32b2b4c` hv: remove dead code
- :acrn-commit:`42e38dfb` hv: fix "No prototype for non-static function"
- :acrn-commit:`48b3cd92` hv: fix "Expression is not boolean"
- :acrn-commit:`11102cfa` hv: change the param type of mmio_write**
- :acrn-commit:`daaff433` doc: upload the images of UP2's serial port
- :acrn-commit:`592bd513` doc: update the serial port part of UP2
- :acrn-commit:`dd43f3ba` hv: replace CPU_PAGE_MASK with PAGE_MASK
- :acrn-commit:`0f766ca6` hv: replace CPU_PAGE_SHIFT with PAGE_SHIFT
- :acrn-commit:`2f15d356` hv: replace CPU_PAGE_SIZE with PAGE_SIZE
- :acrn-commit:`e8e25bd6` hv: clean up function definitions in sbuf.h
- :acrn-commit:`e7d1cdd9` HV: remove ignored pci device from acrn.conf
- :acrn-commit:`e2d09398` DM: remove ignored pci device from SOS bootargs
- :acrn-commit:`db4254e2` HV: find and hide serial PCI dev from service OS
- :acrn-commit:`8d08ec30` HV: replace serial PCI MMIO base with BDF config
- :acrn-commit:`10bde520` hv: other: fix "Procedure has more than one exit point"
- :acrn-commit:`fe3de679` hv: debug: fix "Procedure has more than one exit point"
- :acrn-commit:`414860fb` hv: dev: fix "Procedure has more than one exit point"
- :acrn-commit:`ba44417d` hv: lib: fix "Procedure has more than one exit point"
- :acrn-commit:`279808b2` hv: memory: fix "Procedure has more than one exit point"
- :acrn-commit:`ddb54836` hv: cpu: fix "Procedure has more than one exit point"
- :acrn-commit:`7f08ad83` use 4 vqs
- :acrn-commit:`33362968` change the vq count and vendor id
- :acrn-commit:`d495732c` hv: remove unused flags related APIs in sbuf
- :acrn-commit:`aa9af273` modularization: boot component
- :acrn-commit:`b54f2331` modularization: boot component -- move functions
- :acrn-commit:`51bfafd6` modularization: boot component -- move functions
- :acrn-commit:`512dbb61` Kconfig: remove PLATFORM configuration option
- :acrn-commit:`7eeeccdf` Documentation: add more Kconfig options documentation
- :acrn-commit:`e1564edd` hv: fix type conversion violations
- :acrn-commit:`a0582c99` hv: trusty: refine trusty memory region mapping
- :acrn-commit:`bd1c0838` hv: trusty: reserve memory for trusty
- :acrn-commit:`9bf7dd5d` Enable audio virtualization for AaaG
- :acrn-commit:`79bf121e` hv: throw GP for MSR accesses if they are disabled from guest CPUID
- :acrn-commit:`3836d309` hv: code cleanup: vmsr.c
- :acrn-commit:`36ba7f8a` hv: clear CPUID.07H.EBX[2] to disable SGX from guests
- :acrn-commit:`26dc54ce` HV: allow disabling serial port via Kconfig
- :acrn-commit:`584f6b72` doc: replace return with retval
- :acrn-commit:`97eb72a4` doc: always use 'None' for functions not returning a value
- :acrn-commit:`cbe1b74e` HDCP virtio back-end driver
- :acrn-commit:`fa012e69` CoreU virtio back-end driver
- :acrn-commit:`7003afbe` hv: msix: fix bug when check if msix table access
- :acrn-commit:`5dcfc133` hv:Rename ptdev to ptirq for some APIs
- :acrn-commit:`5b43aa8a` hv:Rename ptdev to ptirq for some variables and structures
- :acrn-commit:`10afa9bb` HV: io: obsolete the valid field in vhm requests
- :acrn-commit:`db3c5746` hv: fix 'Function return value potentially unused'
- :acrn-commit:`e0260b44` doc: add sphinx extension improving only directive
- :acrn-commit:`0bc85d2e` modularization: boot component - move files
- :acrn-commit:`667e0444` hv: vpic: fix "Procedure has more than one exit point"
- :acrn-commit:`17a6d944` hv: guest: fix "Procedure has more than one exit point"
- :acrn-commit:`c32d41a0` hv: irq: fix "Procedure has more than one exit point"
- :acrn-commit:`8dfb9bd9` hv: dm: fix "Procedure has more than one exit point"
- :acrn-commit:`ab3d7c87` hv: boot: fix "Procedure has more than one exit point"
- :acrn-commit:`a1ac585b` hv: add brackets to make operator expression more readable
- :acrn-commit:`aefe9168` Update 'launch_uos.sh' script for UEFI platforms
- :acrn-commit:`839680f0` DM: build TPM2 ACPI table when TPM device enabled
- :acrn-commit:`aae70db6` DM: Add support for virtual TPM enabling
- :acrn-commit:`7df90a25` DM: Support TPM2 CRB device virtualization
- :acrn-commit:`4b83e37c` DM: tpm emulator to communicate with swtpm
- :acrn-commit:`1ba7cebb` Update tools/README.rst
- :acrn-commit:`419feb1a` Documentation: add a README.rst to the tools/ folder
- :acrn-commit:`6d6c5b95` [doc] Enhance Using partition mode on UP2 tutorial
- :acrn-commit:`d3d474cf` Documentation generation: update kernel-doc script to latest
- :acrn-commit:`2d2f96af` hv: clean up function definitions in profiling.h
- :acrn-commit:`14f30a23` hv: clean up function definitions in npk_log.h
- :acrn-commit:`07956605` hv: clean up function definitions in trace.h
- :acrn-commit:`637326bc` hv: clean up function definitions in vuart.h
- :acrn-commit:`7b74b2b9` hv: clean up function definitions in console.h
- :acrn-commit:`649d0e32` hv: clean up function definitions in dump.h
- :acrn-commit:`8920fbac` hv: clean up function definitions in logmsg.h
- :acrn-commit:`5b6c611a` hv: msix: fix "Procedure has more than one exit point"
- :acrn-commit:`2f33d1bc` tools: acrn-manager: Fix acrnctl mistake displaying suspended to paused
- :acrn-commit:`e1d0f7e4` hv: instr_emul: fix decode_modrm no default case in switch statement
- :acrn-commit:`042c3935` hv: trusty: fix get_max_svn_index return type inconsistent
- :acrn-commit:`c200c984` hv: include: remove name starts with underscore
- :acrn-commit:`0100b5a2` HV: replace dynamic memory with static for crypto library
- :acrn-commit:`2afa7173` hv: vlapic: fix "Procedure has more than one exit point"
- :acrn-commit:`3d1332f3` tools: acrn-crashlog: refine the log storage
- :acrn-commit:`06efc58a` hv: assign: clean up HV_DEBUG usage related to vuart pin
- :acrn-commit:`c380ee9e` hv:Revise sanitized page size
- :acrn-commit:`a5fd5524` debug: rename struct logmsg to struct acrn_logmsg_ctl
- :acrn-commit:`e555f75b` debug: Remove early logbuf support
- :acrn-commit:`9f13a51e` hv: hypercall: VM management fix "Procedure has more than one exit point"
- :acrn-commit:`a7398e8a` hv: hypercall: general fix "Procedure has more than one exit point"
- :acrn-commit:`b627c2c9` hv: switch IA32_TSC_AUX between host/guest through VM Controls
- :acrn-commit:`d0b37f8e` hv: reloc: define data structure and MACRO when necessary
- :acrn-commit:`d043171d` IOC mediator: Add VehicalSteeringWheelAngle signal to the whitelist
- :acrn-commit:`580579a3` dm: mei: Use compare and swap primitive for refcnt.
- :acrn-commit:`b1047224` hv: assign: clean up HV_DEBUG usage related to shell
- :acrn-commit:`f21e36f4` hv: vioapic: clean up HV_DEBUG usage
- :acrn-commit:`a9312298` hv: irq: clean up HV_DEBUG usage
- :acrn-commit:`dfe48811` hv: vcpu: clean up HV_DEBUG usage
- :acrn-commit:`e49929a7` hv: ioapic: clean up HV_DEBUG usage
- :acrn-commit:`9d529fb9` hv:use copy of guest's memory block in 'hcall_set_vm_memory_regions()'
- :acrn-commit:`81db2422` hv: enhance Makefile to compile debug/release into 2 libraries
- :acrn-commit:`19b35f97` acrn-dm: wait for monitor thread canceling finish
- :acrn-commit:`02a89dd4` hv: lapic: fix a theoretic infinite loop when clear lapic isr
- :acrn-commit:`dbe3d986` hv: lapic: save lapic base MSR when suspend
- :acrn-commit:`b8a553d1` hv: lapic: remove union apic_lvt
- :acrn-commit:`538ba08c` hv:Add vpin to ptdev entry mapping for vpic/vioapic
- :acrn-commit:`297a264a` hv:Cleanup ptdev lock
- :acrn-commit:`b7bbf812` hv:Replace dynamic memory with static for ptdev
- :acrn-commit:`b0e1657b` HV: Adding partition mode support for cb2_dnv
- :acrn-commit:`664bc1ba` HV: Partition mode source code file layout reorg
- :acrn-commit:`031191db` dm: apl-mrb: launch_uos: remove mei debug flags
- :acrn-commit:`378afc50` dm: mei: fix clients scan in sysfs
- :acrn-commit:`7bd2976f` dm: mei: add zero termination to devpath
- :acrn-commit:`87fbb700` dm: mei: fix double fw_reset on uos reboot
- :acrn-commit:`844553ef` dm: mei: check for state before link reset callback
- :acrn-commit:`58ab26ea` hv: code cleanup: msr.h
- :acrn-commit:`e8296dce` hv: Add IO request completion polling feature
- :acrn-commit:`e350abe4` HV: handle adding ptdev entry failure cases
- :acrn-commit:`fe08a44e` hv: doc: use doxygen-generated API docs in HLD for passthru
- :acrn-commit:`973c616a` doc: passthru: add structure and API docs
- :acrn-commit:`4ec4ddc0` Documentation: clean-up of isolated README.rst files
- :acrn-commit:`29f95021` hv: vtd: error handling revisit
- :acrn-commit:`c4490028` hv: vtd: defer dma remapping enabling until vm creation
- :acrn-commit:`42e0e169` hv: vtd: init interrupt config when resume
- :acrn-commit:`830b3aa0` hv: vtd: check vtd enabling status with spinlock
- :acrn-commit:`a2cb9c2b` hv: vtd: add do_action_for_iommus
- :acrn-commit:`32ed3d1a` hv: vtd: move public API to the bottom part of the file
- :acrn-commit:`efb24923` hv: vtd: merge lines for a statement if needed.
- :acrn-commit:`e35a8e8c` hv: vtd: typo fix
- :acrn-commit:`611944c0` dm: Fix race between ioreq client destroy and access
- :acrn-commit:`3b24c34c` hv: msix: corrently determine when the guest is changing Message Data/Addr
- :acrn-commit:`c41c0dab` hv: properly initialize MSI-X table
- :acrn-commit:`53971e19` hv: fix 2 bugs in msix.c
- :acrn-commit:`119eccfe` hv: hypercall: clean up HV_DEBUG usage
- :acrn-commit:`fc9ec5d8` hv: Derive decryption key from Seed for Trusty to decrypt attestation keybox
- :acrn-commit:`7978188c` tools: acrn-manager: set MAX_NAME_LEN to 32
- :acrn-commit:`5d013ed2` hv: vpci: revert the temporary workaround of handling I/O port CF9
- :acrn-commit:`fe9a340e` hv: separate the PCI CONFIG_ADDR and CONFIG_DATA I/O port handlers
- :acrn-commit:`8b4f3956` hv: PIO emulation handler is attached to I/O port number only
- :acrn-commit:`2c581751` vmx: tiny fix for MACRO name and print format
- :acrn-commit:`9c025190` hv: hv_main: clean up HV_DEBUG usage
- :acrn-commit:`1018a31c` HV: For NUC, use 0x3F8/IRQ4 as the vuart port base address/IRQ and use ttyS0 accordingly
- :acrn-commit:`e56a6b58` HV: For MRB, use 0x3E8/IRQ6 as the vuart port base address/IRQ.
- :acrn-commit:`3b87e7c6` HV: Add vuart port base address/IRQ Kconfig options
- :acrn-commit:`b32e689a` hypervisor: use "wbinvd" carefully in RT environment
- :acrn-commit:`61e6c1f0` hv: reset VM ioreqs in reset_vm
- :acrn-commit:`2fa67a44` HV: clear memory region used by UOS before it exit
- :acrn-commit:`8fa16211` hv: partition mode also needs free vm id when shutdown vm
- :acrn-commit:`9dacc4a5` tools:acrn-crashlog:fix potential issue
- :acrn-commit:`6971cc83` hv: fix '(void) missing for discarded return value'
- :acrn-commit:`a646fcf7` hv: fix 'No brackets to then/else' in vpci code
- :acrn-commit:`bad813ea` hv: fix MISRA-C violations in vpci code: Function pointer is of wrong type
- :acrn-commit:`d3f0edfe` hv: fix MISRA-C violations in vpci code: 93S, 331S and 612S
- :acrn-commit:`f84f1a21` hv: fix MISRA-C violations in vpci code: implicit conversion
- :acrn-commit:`d97224a4` hv: fix integer violations
- :acrn-commit:`7e6d0a21` HV:pic fix "Recursion in procedure calls found"
- :acrn-commit:`0a9d6841` hv: instr_emul: clean up mmio_read/mmio_write
- :acrn-commit:`2c7c909e` hv: vtd: fix the pre-allocated context table number
- :acrn-commit:`3731b4c0` hv: fix '(void) missing for discarded return value'
- :acrn-commit:`b3b24320` hv: fix possible inconsistent issue for 'vm->hw.create_vcpus'
- :acrn-commit:`7bb09f75` fix "Procedure is not pure assembler"
- :acrn-commit:`91fb441d` hv:add global lock for VM & vCPU management hypercalls
- :acrn-commit:`15567535` hv: unify the function pointer assignment
- :acrn-commit:`9a009bce` hv:Replace dynamic memory with static for mmio
- :acrn-commit:`b5505c43` doc: tweaks to 0.3 release notes
- :acrn-commit:`a0345279` DM: update User OS launch script with v0.3 CL and kernel numbers
- :acrn-commit:`1847497d` Documentation: update GSG for release 0.3
- :acrn-commit:`33137dc9` Documentation: adjust "Tracked-On" capitalization in documentation
- :acrn-commit:`f0ec5b26` doc: add Makefile option for singlehtml
- :acrn-commit:`d62196ac` version: 0.4-unstable

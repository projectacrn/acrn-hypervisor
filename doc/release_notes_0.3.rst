.. _release_notes_0.3:

ACRN v0.3 (Nov 2018)
####################

We are pleased to announce the release of Project ACRN version 0.3.

ACRN is a flexible, lightweight reference hypervisor, built with
real-time and safety-criticality in mind, optimized to streamline
embedded development through an open source platform. Check out the
:ref:`introduction` for more information.


All project ACRN source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, and documentation.
You can either download this source code as a zip or tar.gz file (see
the `ACRN v0.3 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v0.3>`_ or
use Git clone and checkout commands:

.. code-block:: bash

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v0.3

The project's online technical documentation is also tagged to correspond
with a specific release: generated v0.3 documents can be found at
https://projectacrn.github.io/0.3/.  Documentation for the latest
(master) branch is found at https://projectacrn.github.io/latest/.


Version 0.3 new features
************************


- :acrn-issue:`866` - Security Interrupt Storm Mitigation
- :acrn-issue:`878` - Virtualization HLD
- :acrn-issue:`887` - Security xD support
- :acrn-issue:`944` - CSME (and subcomponent) Sharing
- :acrn-issue:`946` - CS(M)E Mediator Definition
- :acrn-issue:`951` - Device CS(M)E support
- :acrn-issue:`1122` - Security Enable compiler and linker setting-flags to harden software
- :acrn-issue:`1124` - MMU code reshuffle
- :acrn-issue:`1179` - RPMB key passing
- :acrn-issue:`1180` - vFastboot release version 0.9
- :acrn-issue:`1181` - Integrate enabling Crash OS feature as default in VSBL debug Version
- :acrn-issue:`1182` - vSBL to support ACPI customization
- :acrn-issue:`1213` - IOC Mediator added RTC Timer feature
- :acrn-issue:`1230` - fix the %l format given to print API only print 32bit
- :acrn-issue:`1231` - VM loader reshuffle
- :acrn-issue:`1240` - [APL][IO Mediator] Enable VHOST_NET & VHOST to accelerate guest networking with virtio_net.
- :acrn-issue:`1284` - [DeviceModel]Enable NHLT table in DM for audio passthrough
- :acrn-issue:`1313` - [APL][IO Mediator] Remove unused netmap/vale in virtio-net
- :acrn-issue:`1328` - [APL][IO Mediator] change trace_printk to pr_debug for vhm ioctl
- :acrn-issue:`1329` - ioeventfd and irqfd implementation to support vhost on ACRN
- :acrn-issue:`1343` - Enable -Werror for ACRN hypervisor
- :acrn-issue:`1364` - [APL][IO Mediator]  virtio code reshuffle
- :acrn-issue:`1369` - allocate more RAM to UOS on MRB.
- :acrn-issue:`1401` - IOC mediator reshuffle
- :acrn-issue:`1420` - Update contributing doc with Tracked-On requirement for commits
- :acrn-issue:`1455` - x2apic support for acrn
- :acrn-issue:`1616` - remove unused parameters for acrn-dm
- :acrn-issue:`1626` - support x2APIC mode for ACRN guests
- :acrn-issue:`1672` - L1TF mitigation
- :acrn-issue:`1701` - MISRA C compliance Naming Convention
- :acrn-issue:`1711` - msix.c use MMIO read/write APIs to access MMIO registers

Fixed Issues
************

- :acrn-issue:`1209` - specific PCI device failed to passthrough to UOS
- :acrn-issue:`1268` - GPU hangs when running GfxBench Car Chase in SOS and UOS.
- :acrn-issue:`1270` - SOS and UOS play video but don't display video animation output on monitor.
- :acrn-issue:`1339` - SOS failed to boot with SSD+NVMe boot devices on KBL NUC
- :acrn-issue:`1432` - SOS failed boot

Known Issues
************

:acrn-issue:`677` - SSD Disk ID is not consistent between SOS/UOS
   The SSD disk ID in the UOS is not the same as in the SOS when the SSD
   device is passed-through to the UOS (it should be). The ID is also
   changing after a reboot (it shouldn't). **Impact:** There is no impact
   to functionality. **Workaround:** None. The issues will be fixed in the
   next release.

:acrn-issue:`1319` - SD card pass-through: UOS can't see SD card after UOS reboot.
   SD card could not be found after UOS reboot in pass-through mode.
   **Impact:** There is no SD card after UOS reboot.
   **Workaround:** None. The issue will be fixed in the next release.

:acrn-issue:`1773` - USB Mediator: Can't find all devices when multiple usb devices connected[Reproduce rate:60%]
   After booting UOS with multiple USB devices plugged in, sometimes there
   are one or more devices cannot be discovered. The reproduce rate is ~60%.
   **Impact:** Cannot use multiple usb devices at same time.
   **Workaround:** Plug-out and plug-in the unrecognized device again.

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

:acrn-issue:`1777` - After UOS plays video for several minutes, the UOS image will be stagnant
   After UOS plays video for several minutes, the UOS image will be stagnant.
   **Impact:** UOS cannot play video image smoothly all the time.
   **Workaround:** None. The issues will be fixed in the next release.

:acrn-issue:`1779` - gfxbench cannot run in SOS&UOS
   Failed to run gfxbench in SOS or UOS.
   **Impact:** Cannot run gfxbench in SOS or UOS.
   **Workaround:** None. The issues will be fixed in the next release.

:acrn-issue:`1780` - Some video formats cannot be played in SOS
   There are several formats of videos that cannot be played in SOS:
   H265_10bits, VP8, VP9, VP9_10bits, H265.720p.
   **Impact:** Cannot play those formats of videos in SOS.
   **Workaround:** None. The issues will be fixed in the next release.

:acrn-issue:`1782` - UOS failed to get IP address with the pass-through network card
   After network card is pass-through to UOS, it fails to get IP address in UOS.
   **Impact:** Cannot use network in UOS.
   **Workaround:** None. The issues will be fixed in the next release.

:acrn-issue:`1794` - After SOS boots up, there’s no output on SOS screen
   After SOS boots up with both “desktop” and “soft-defined-cockpit” bundles installed
   or without any, there’s no output on SOS screen.
   **Impact:** Cannot access SOS.
   **Workaround:** Only install “desktop” bundle and enable&start weston in Native ClearLinux,
   and then reboot to SOS. The issues will be fixed in the next release.

:acrn-issue:`1795` - [KBL NUC] SOS fails to get IP address
   On KBL NUC hardware platform, SOS fails to get IP address after SOS boot.
   **Impact:** Cannot use network in SOS.
   **Workaround:** None. The issues will be fixed in the next release.

:acrn-issue:`1796` - APL NUC fails to reboot sometimes
   After APL NUC boot to SOS, type "reboot" to reboot SOS, it fails to reboot sometimes.
   **Impact:** Cannot reboot SOS.
   **Workaround:** Power off and boot again. The issues will be fixed in the next release.


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

These commits have been added to the acrn-hypervisor repo since the v0.2
release in Sep 2018 (click on the CommitID link to see details):

.. comment

   This list is obtained from the command:
   git log --pretty=format:'- :acrn-commit:`%h` %s' --after="2018-03-01"


- :acrn-commit:`b6988e13` hv: fix branch addressing syntax warning
- :acrn-commit:`053608a5` HV: add px cx data of bxt n3350 SOC
- :acrn-commit:`73530055` hv:Replace dynamic memory with static for port io
- :acrn-commit:`5a1f2447` hv: ept: set snp control when modify
- :acrn-commit:`66f133bf` DM: Fix the typo error in checking the /dev/vbs_ipu file
- :acrn-commit:`ab5572bd` doc: explain intr storm monitor params in acrn-dm
- :acrn-commit:`f65e05c5` hv: fix 'Start of variable is upper case'
- :acrn-commit:`d49a6f6f` capture acrnd AaaG booting messages to journald
- :acrn-commit:`c4161c87` dm: uart: fix UOS console output to stdin
- :acrn-commit:`b5881727` DM: add interrupt storm monitor params in cmdline
- :acrn-commit:`ad1cbb76` DM: add interface to set intr storm monitor params
- :acrn-commit:`1902d725` hv: fix partition mode no console issue.
- :acrn-commit:`3cbaf028` HV: Use parameter directly to pass bdf for hcall_assign/deassign_ptdev
- :acrn-commit:`605738fc` hv: hypercall: remove hcall_set_vm_memory_region
- :acrn-commit:`b430b00a` hv: fix 'Expression is not Boolean'
- :acrn-commit:`121454c4` hv: fix a minor bug of static checks
- :acrn-commit:`0800624f` hv: vtd: use pre-defined function for bdf calculation
- :acrn-commit:`039a1c0f` hv: pci: replace fucntion like macro with inline func
- :acrn-commit:`36aaaa1c` DM NPK: unmap the MMIO in pci_npk_deinit
- :acrn-commit:`ef974d1a` hv:Remove atomic operation to set initial value for 'created_vcpu'
- :acrn-commit:`0fc47b5a` hv: fix release build issue
- :acrn-commit:`a8a1e229` hv:vtd: fix minor bug in domain count
- :acrn-commit:`78dd92e4` DM: sos_bootargs: split sos_bootargs into multiple lines
- :acrn-commit:`44ce3a66` doc: update HLD Trace/Log
- :acrn-commit:`27fffb96` doc: update HLD Power Management
- :acrn-commit:`b3d21683` doc: update HLD VM Management
- :acrn-commit:`dfcc06df` doc: update HLD Virtio Devices
- :acrn-commit:`366042ca` hv: fix integer violations
- :acrn-commit:`65a2613a` IOC mediator: fix wakeup reason issue when UOS resuming
- :acrn-commit:`a2516ecc` fix "Casting operation to a pointer"
- :acrn-commit:`ad1e2ab6` DM/sos_bootargs: remove unused parameters for graphics
- :acrn-commit:`ee918f85` HV:debug:profiling Fixed inappropriate condition check
- :acrn-commit:`7f57a74f` hypercall doc: Sync the comments for each hypercall from .h to .c
- :acrn-commit:`99586e32` HV:treewide:rename vm data structure
- :acrn-commit:`ace4f48c` HV:treewide:rename vcpu_arch data structure
- :acrn-commit:`fa26a166` HV:treewide:rename vcpu data structure
- :acrn-commit:`969f7cf0` DM:Check the device file of /dev/vbs_ipu to determine IPU mode
- :acrn-commit:`8efd9edb` hv: vPCI needs to handle reset IO port CF9 differently
- :acrn-commit:`d261b4bc` doc: update virtio related functions doc comments
- :acrn-commit:`ea801a16` dm: Remove unused duplicated API dm_gpa2hva
- :acrn-commit:`4e540e54` HV: doc: use doxygen-generated API docs in HLD
- :acrn-commit:`e0fcb70d` HV: io: add structure and API docs
- :acrn-commit:`a4be10f3` hv: mmu: unmap the trusty EPT when destroy_secure_world
- :acrn-commit:`e8229879` hv: use MMIO read/write APIs to access MMIO registers
- :acrn-commit:`313941e8` tools: acrn-manager: remove unsafe api sscanf
- :acrn-commit:`e24464a0` tools: acrnlog: remove usage of banned APIs
- :acrn-commit:`1d96ce5f` doc: add doyxgen alias for easy reST inclusion
- :acrn-commit:`d2d0cbc6` hv:doc:use doxyen-generated API docs in HDL for vIRQ
- :acrn-commit:`cbe03135` hv: revise interfaces description in virq
- :acrn-commit:`f23606a4` hv: revise interfaces description in vioapic
- :acrn-commit:`7c20cb0c` hv: revise interfaces description in vpic
- :acrn-commit:`c41f2860` hv: revise interfaces description in vlapic
- :acrn-commit:`46949631` Documentation: add 'make' to GSG and expand PATH for `sphinx-build`
- :acrn-commit:`4b3b1155` hv: doc: use doxygen-generated API docs in HLD for vtd
- :acrn-commit:`1776d7e7` hv: vtd: add structure and API docs
- :acrn-commit:`7dc3e609` doc: hv: add comments to irq APIs for documentation
- :acrn-commit:`f69dd1c6` HV:doc:use doxygen-generated API docs in HLD
- :acrn-commit:`7c3c6ea4` HV:MM:add API docs
- :acrn-commit:`17d43fe5` doc: doc: update HLD Emulated Devices
- :acrn-commit:`bf88e241` DOC: add main vcpu API & data structure into HLD.
- :acrn-commit:`c8850114` HV: add main vcpu API comments for document
- :acrn-commit:`277c9330` doc: fix formatting error in l1tf doc
- :acrn-commit:`2c85480c` doc: format l1tf.rst
- :acrn-commit:`d6247ff7` doc: update l1tf.rst line endings setting to unix style
- :acrn-commit:`eefb06b3` hv: mmu: add 16GB RAM support for uefi platform
- :acrn-commit:`c36f4d27` doc: hotfix build issue blocked by l1tf.rst
- :acrn-commit:`48ae379b` hv: LAPIC pass-thru support for partition mode of ACRN
- :acrn-commit:`ff56b6f6` hv: Add support for leaf 0xb emulation
- :acrn-commit:`f3aa20a8` hv: self-IPI APIC register in x2APIC mode of guest vLAPIC
- :acrn-commit:`c85e35d3` hv: Switch APICv from MMIO to MSR for x2APIC mode of guest vLAPIC
- :acrn-commit:`cf4d1912` hv: Modify vlapic_get_apicid for x2APIC mode of vLAPIC
- :acrn-commit:`80b6e627` hv: Add APIs to convert x2APIC MSR accesses to LAPIC MMIO offset
- :acrn-commit:`e9fe6efd` hv: vLAPIC ICR write and destination mask  matching for x2APIC
- :acrn-commit:`6a4dcce3` hv: APIs for building x2APIC ID and LDR
- :acrn-commit:`7ecc521c` hv: Modify enable_msr_interception API
- :acrn-commit:`64f61961` hv: add missing support to intercept x2APIC MSRs
- :acrn-commit:`94dadc1d` dm: virtio-input: ignore MSC_TIMESTAMP from guest
- :acrn-commit:`ed113f57` hv: mmu: remove "##" for MISRA C
- :acrn-commit:`541f3713` hv: bug fix: normal world may get trusty world's pdpt page
- :acrn-commit:`f1ed6c50` hv: mmu: remove alloc_page() API
- :acrn-commit:`0391f84c` hv: mmu: replace dynamic memory allocation in memory
- :acrn-commit:`9c7c0de0` hv: mmu: add static paging table allocation for EPT
- :acrn-commit:`dc9d18a8` hv: mmu: add static paging table allocation for hypervisor
- :acrn-commit:`74a5eec3` DM: change SOS bootargs console ttyS0 to ttyS2
- :acrn-commit:`0307b218` HV: change vuart port (used by SOS) to ttyS2
- :acrn-commit:`9029ac4b` doc: update Tracked-on in contribute guide
- :acrn-commit:`a86248ec` doc: hide doxygen duplicate definition warnings
- :acrn-commit:`3ffa9686` tools: acrn-crashlog: fix potential issues
- :acrn-commit:`111f9726` hv: fix integer violations
- :acrn-commit:`4c1cb606` hv: Remove the up_count_spinlock and use atomic for up_count
- :acrn-commit:`b7472063` HV: add size check for shell log buffer usage
- :acrn-commit:`b048835c` HV: fix bug "vmexit" cmd cause HV console hung
- :acrn-commit:`0255e627` hv: resolve the negative impacts to UOS MSI/MSI-X remapping
- :acrn-commit:`c1d2499e` hv: enable MSI remapping on vm0
- :acrn-commit:`8c398f7d` hv: fix issues when msi-x shares same BAR with other data structures
- :acrn-commit:`5cbe079e` hv: MSI-X Message Address write fix
- :acrn-commit:`dbe156e9` hv: fix misrac violations in vcpi code
- :acrn-commit:`5555a2f8` hv: fix bug in sizing MSI-X Table Base Address Register
- :acrn-commit:`51977a6d` hv: Don't check multi-function flag in PCI enumeration
- :acrn-commit:`e32bc9e3` hv: avoid hardcode cs.limit in set_vcpu_regs()
- :acrn-commit:`0cd85749` HV: save the cs limit field for SOS
- :acrn-commit:`6993fdb3` DM: set cs_limit from DM side for UOS
- :acrn-commit:`b12c7b74` tools: acrn-manager: remove usage of banned APIs
- :acrn-commit:`af760f8d` tools: acrn-manager: refine the usage of api 'snprintf'
- :acrn-commit:`5493804c` tools: acrnlog: refine the usage of api 'snprintf'
- :acrn-commit:`a2383b06` tools: acrntrace: remove unsafe api and return value check for snprintf
- :acrn-commit:`2975f9fa` hv:Replace dynamic memory with static for sbuf
- :acrn-commit:`9e397322` hv: l1tf: sanitize mapping for idle EPT
- :acrn-commit:`fb68468c` HV: flush L1 cache when switching to normal world
- :acrn-commit:`34a63365` HV: enable L1 cache flush when VM entry
- :acrn-commit:`d43d2c92` HV: add CPU capabilities detection for L1TF mitigation
- :acrn-commit:`2731628e` HV: wrap security related CPU capabilities checking
- :acrn-commit:`25c2d4d7` doc: add l1tf document
- :acrn-commit:`b0cac0e6` Samples:Added the Kernel console parameter in boot.
- :acrn-commit:`43f6bdb7` hv: vtd: fix device assign failure for partition mode
- :acrn-commit:`9ae79496` doc: fix section heading in device model hld
- :acrn-commit:`7df70e0c` doc: update HLD Device Model
- :acrn-commit:`390cc678` doc: tweak doxygen/known-issues handling
- :acrn-commit:`60d0a752` hv: fix integer violations
- :acrn-commit:`4d01e60e` hv: vtd: remove dynamic allocation for iommu_domain
- :acrn-commit:`dda08957` hv: vtd: remove dynamic allocation for dmar_drhd_rt
- :acrn-commit:`f05bfeb9` hv: vtd: remove dynamic page allocation for root&ctx table
- :acrn-commit:`1b1338bc` snprintf: Remove the %o and %p support
- :acrn-commit:`6150c061` dm: bios: update to version 1.0.1
- :acrn-commit:`8c7d471c` HV: bug fix:possible access to NULL pointer
- :acrn-commit:`9ba75c55` dm: mei: fix firmware reset race.
- :acrn-commit:`5f41d4a8` dm: mei: check return value of vmei_host_client_to_vmei()
- :acrn-commit:`b4fbef46` dm: mei: destroy mutex attribute on error path
- :acrn-commit:`8abc9317` dm: mei: set addresses in the hbm disconnect reply
- :acrn-commit:`6bb3d048` hv: remove deprecated functions declartion
- :acrn-commit:`a0ace725` DM USB: xHCI: fix process logic of TRB which has zero data length
- :acrn-commit:`6266dd01` DM: correct memory allocation size for UOS
- :acrn-commit:`ac5b46eb` doc: update rest of hypervisor HLD sections
- :acrn-commit:`97c8c16f` doc: fix misspellings in hld docs
- :acrn-commit:`569ababd` hv: switch vLAPIC mode vlapic_reset
- :acrn-commit:`48d8123a` devicemodel:nuc:launch_uos.sh: drop a useless clear parameter
- :acrn-commit:`62a42d5f` devicemodel: Makefile: clean up/refactor some code
- :acrn-commit:`df5336c9` gitignore: drop some useless entries
- :acrn-commit:`7169248b` sos_bootargs_release.txt: enable guc firmware loading
- :acrn-commit:`fdf1a330` sos_bootargs_debug.txt: enable guc firmware loading
- :acrn-commit:`8873859a` kconfig: optionally check if the ACPI info header is validated
- :acrn-commit:`5f6a10f1` kconfig: use defconfig instead of default values in silentoldconfig
- :acrn-commit:`b9d54f4a` kconfig: support board-specific defconfig
- :acrn-commit:`8bde372c` kconfig: enforce remaking config.mk after oldconfig changes .config
- :acrn-commit:`c7907a82` kconfig: a faster way to check the availability of python3 package
- :acrn-commit:`256108f1` kconfig: add more help messages to config symbols
- :acrn-commit:`05bb7aa2` hv: remove deprecated hypercalls
- :acrn-commit:`bf7b1cf7` doc: update HLD Device passthrough
- :acrn-commit:`7c192db1` doc: update HLD VT-d
- :acrn-commit:`e141150e` doc: Fix AcrnGT broken API doc due to kernel upgrade
- :acrn-commit:`83dbfe4f` hv: implement sharing_mode.c for PCI emulation in sharing mode
- :acrn-commit:`7c506ebc` hv: implement msix.c for MSI-X remapping
- :acrn-commit:`dcebdb8e` hv: implement msi.c to handle MSI remapping for vm0
- :acrn-commit:`6af47f24` hv: vpci: add callback functions to struct vpci
- :acrn-commit:`3e54c70d` hv: rework the MMIO handler callback hv_mem_io_handler_t arguments
- :acrn-commit:`ec5b90f1` hv: implement PCI bus scan function
- :acrn-commit:`9cc1f57f` hv: change function parameters: pci_pdev_read_cfg and pci_pdev_write_cfg
- :acrn-commit:`19e1b967` hv: MSI Message Address should be 64 bits
- :acrn-commit:`7b4b78c3` hv: minor cleanup for dm/vpci code
- :acrn-commit:`bc4f82d1` hv: more cleanup for pci.h
- :acrn-commit:`e24899d9` fix "Recursion in procedure calls found"
- :acrn-commit:`e8a59f30` checkpatch: fix the line limit back to 120
- :acrn-commit:`f4f139bf` DM: generate random virtual RPMB key
- :acrn-commit:`dff441a0` hv:Replace dynamic memory with static for pcpu
- :acrn-commit:`4afb6666` hv:cleanup vcpu_id compare with phys_cpu_num
- :acrn-commit:`3eb45b9b` hv:Check pcpu number to avoid overflow
- :acrn-commit:`672583a0` hv: Check pcpu number in Hw platform detect
- :acrn-commit:`298044d9` hv: Add MAX_PCPU_NUM in Kconfig
- :acrn-commit:`b686b562` DM: wrap ASSERT/DEASSERT IRQ line with Set/Clear IRQ line
- :acrn-commit:`e12f88b8` dm: virtio-console: remove unused virtio_console_cfgwrite
- :acrn-commit:`7961a5ba` HV: Fix some inconsistent comments in vm_description.c
- :acrn-commit:`8860af3b` dm: fix possible buffer overflow in 'acrn_load_elf()'
- :acrn-commit:`dc7df1cd` doc: update HLD Virtual Interrupt
- :acrn-commit:`1c54734f` doc: update HLD Timer section
- :acrn-commit:`d6523964` Documentation: tweak 'partition mode' tutorial
- :acrn-commit:`b3cb7a53` Fix to kernel hang in smp_call_function
- :acrn-commit:`cab93c05` HV:Added SBuf support to copy samples generated to guest.
- :acrn-commit:`5985c121` HV:Added implementation for PMI handler function
- :acrn-commit:`a7cbee18` HV:Added support to get VM enter and exit information
- :acrn-commit:`fc8f9d79` HV:Added support to perform MSR operation on all cpus
- :acrn-commit:`1786f622` HV:Added support to setup tool & start/stop profing
- :acrn-commit:`898b9c8d` HV:Added support to configure PMI and VM switch info
- :acrn-commit:`df549096` HV:Added support to get phy CPU, VM, tool information
- :acrn-commit:`8ba333d2` HV: Added Initial support for SEP/SOCWATCH profiling
- :acrn-commit:`3010718d` dm: cmdline: remove unused parameters
- :acrn-commit:`4261ca22` DM USB: xHCI: refine logic of Disable Slot Command
- :acrn-commit:`e1e0d304` DM USB: xHCI: refine the USB disconnect logic in DM
- :acrn-commit:`f799e8fa` DM USB: xHCI: fix process logic of LINK type TRB
- :acrn-commit:`08a7227f` DM USB: xHCI: fix bug in port unassigning function
- :acrn-commit:`d7008408` DM USB: xHCI: fix issue: crash when plug device during UOS booting
- :acrn-commit:`3d94f868` hv: flush cache after update the trampoline code
- :acrn-commit:`0166ec4b` HV: debug: Check if vUART is configured in partition mode
- :acrn-commit:`05834927` HV: Fix boot failure of partition mode
- :acrn-commit:`70e13bf8` doc: update interrupt hld section
- :acrn-commit:`f84547ca` doc: move docs to match HLD 0.7 org
- :acrn-commit:`9871b343` doc: update I/O emulation section
- :acrn-commit:`6dffef12` doc: filter error exit status incorrect
- :acrn-commit:`d764edbf` doc: update GRUB menu image in partition mode doc
- :acrn-commit:`61a9ca20` Documentation: Add tutorial about how to use partition mode on UP2
- :acrn-commit:`bc7b06ae` doc: update Memory management HLD
- :acrn-commit:`2f8c31f6` tools: acrn-crashlog: update the documents
- :acrn-commit:`655132fc` dm: virtio: remove unused vbs_kernel_init
- :acrn-commit:`eb265809` DM: multiboot info address in DM for elf loader is wrong.
- :acrn-commit:`80e02c97` DM USB: xHCI: Fix an potential array out of range issue.
- :acrn-commit:`84c0c878` DM USB: xHCI: Fix a potential NULL pointer issue.
- :acrn-commit:`1568a4c0` hv:Remove deadcode 'vm_lapic_from_pcpuid'
- :acrn-commit:`46d19824` HV:vcpu fix "Pointer param should be declared pointer to const"
- :acrn-commit:`ea32c34a` HV:fix "Pointer param should be declared pointer to const"
- :acrn-commit:`d79007ae` HV:add const to bitmap_test parameter addr
- :acrn-commit:`482a4dcc` DM: correct the predefine DM option string.
- :acrn-commit:`85bec0d0` hv: Move the guest_sw_loader() call from vcpu to vm
- :acrn-commit:`4f19b3b8` hv: Prepare the gdt table for VM
- :acrn-commit:`ad1ef7ba` samples: enable pstore via the sos kernel cmdline
- :acrn-commit:`6d076caa` tools: acrn-crashlog: remove unsafe apis in usercrash
- :acrn-commit:`8f7fa50d` hv: fix mapping between GSI Num#2 and PIC IRQ #0
- :acrn-commit:`96f8becc` dm: bios: update vSBL to v1.0
- :acrn-commit:`09193c39` hv: x2apic support for acrn
- :acrn-commit:`19abb419` launch_uos.sh: make sure cpu offline by retry
- :acrn-commit:`241d5a68` HV: fix bug by improving intr delay timer handling
- :acrn-commit:`4228c81b` DM: compare unsigned numbers to avoid overflow.
- :acrn-commit:`d2993737` tools: acrnd: Stop all vms when SOS shutdown/reboot
- :acrn-commit:`7b06be9e` HV: checkpatch: add configurations to customize checkpatch.pl
- :acrn-commit:`7195537a` dm: virtio-net: replace banned functions
- :acrn-commit:`7579678d` dm: add const declaration for dm_strto* APIs
- :acrn-commit:`bd97e5cb` dm: rpmb: Support RPMB mode config from launch.sh
- :acrn-commit:`107eaa3a` HV:fix MACRO value mismatch
- :acrn-commit:`a853c055` tools: acrnctl: fix: resume default wakeup reason is CBC_WK_RSN_BTN
- :acrn-commit:`a6677e6e` hv: create new file core.c and pci.c
- :acrn-commit:`4741fcff` hv: pci_priv.h code cleanup
- :acrn-commit:`a43ff9ce` hv: timer: add debug information for add_timer
- :acrn-commit:`7ca1a7de` dm: launch_uos.sh add virtio_mei mediator
- :acrn-commit:`201e5cec` dm: mei: enable virtio_mei compilation
- :acrn-commit:`d4b9bd59` dm: mei: add module initialization
- :acrn-commit:`f6e6e858` dm: mei: implement vmei_start/stop()
- :acrn-commit:`6a1f8242` dm: mei: implement rx flow.
- :acrn-commit:`50ecd93b` dm: mei: implement tx flow
- :acrn-commit:`483a893e` dm: mei: implement HBM protocol handler
- :acrn-commit:`98c6b7a6` dm: mei: add native io handlers
- :acrn-commit:`3abbf10e` dm: mei: add me clients enumeration
- :acrn-commit:`7cbb3872` dm: mei: add virtio cfgread/cfgwrite handlers.
- :acrn-commit:`f462601b` dm: mei: add reset handlers
- :acrn-commit:`a632ac3d` dm: mei: add client management infrastructure
- :acrn-commit:`445f4193` dm: mei: add virtio configuration
- :acrn-commit:`0dc7adfb` dm: mei: add sysfs read functions
- :acrn-commit:`b8d53d17` dm: mei: add reference counter functions
- :acrn-commit:`6a96878e` dm: types: add container_of macro
- :acrn-commit:`4e057c32` dm: mei: add guid handling functions
- :acrn-commit:`d141aebd` dm: mei: add mei hbm protocol definitions header.
- :acrn-commit:`0cc50b1d` dm: remove virtio_heci
- :acrn-commit:`39fde060` hv: ept: remove EPT paging table for HPA to GPA
- :acrn-commit:`70ddca3a` hv: mmu: add pre-assumption for hpa2gpa
- :acrn-commit:`49b476bb` hv: vm_load: set zeropage just past boot args
- :acrn-commit:`9368373f` tools: acrn-crashlog: check the pointer after getting sender
- :acrn-commit:`2973db78` DM: VMcfg: generated example header
- :acrn-commit:`c86da003` DM: VMcfg: support --dump options
- :acrn-commit:`67d72920` DM: VMcfg: support --vmcfg options
- :acrn-commit:`321021eb` DM: VMcfg: mrb-env-setup.sh
- :acrn-commit:`d2ed9955` DM: VMcfg: support VM1 on MRB
- :acrn-commit:`ae5b32dc` DM: VMcfg: build-in vm configurations
- :acrn-commit:`646cc8c4` DM: VMcfg: Kconfig & Makefile for VM Configuration
- :acrn-commit:`4ce80e5c` tools: acrn-manager: fix a potential compiler warning
- :acrn-commit:`e8c86566` tools: acrn-manager: fix a potential NULL pointer dereference
- :acrn-commit:`da3b0270` tools: acrnd: Ignore null line reading from timer_list
- :acrn-commit:`a45d961b` tools: acrnd: check weakup reason first in init_vm
- :acrn-commit:`acc51877` doc: merge static core with cpu virt
- :acrn-commit:`e01f4777` doc: HV startup and CPU virtualization sections
- :acrn-commit:`8893a8c2` doc: update HLD overview chapter
- :acrn-commit:`60b216a4` HV:fixed "Pointer param should be declared pointer to const"
- :acrn-commit:`40dbdcde` tools: acrn-crashlog: remove unsafe strlen in common
- :acrn-commit:`f25bc50e` tools: acrn-crashlog: update string operation in acrnprobe
- :acrn-commit:`6938caa2` tools: acrn-crashlog: refine the configuration structure
- :acrn-commit:`fe4d503c` tools: acrn-crashlog: remove unsafe api sscanf
- :acrn-commit:`fb029284` tools: acrn-crashlog: remove unsafe api sprintf
- :acrn-commit:`5ecf1078` tools: acrn-crashlog: remove unsafe apis from android_events.c
- :acrn-commit:`48ce01a5` tools: acrn-crashlog: new api in strutils
- :acrn-commit:`6a9a46ac` DM USB: xHCI: workaround for Stop Endpoint Command handling
- :acrn-commit:`ecf0585b` DM USB: xHCI: fix incorrect device searching logic
- :acrn-commit:`6b2a18a8` DM USB: add support for multi-layers hubs
- :acrn-commit:`f533a07a` DM USB: xHCI: support multiple hubs in single layer
- :acrn-commit:`6886d3cd` DM USB: xHCI: change port mapping logic for multiple hub support
- :acrn-commit:`540ce05f` DM USB: introduce function usb_get_native_devinfo
- :acrn-commit:`e8f7b6fa` DM USB: introduce struct usb_devpath and releted functions
- :acrn-commit:`14bc961f` DM USB: xHCI: remove old hub support code.
- :acrn-commit:`8b5d357f` HV: move default ACPI info to default_acpi_info.h
- :acrn-commit:`bd042352` hv: fix potential buffer overflow in vpic_set_pinstate()
- :acrn-commit:`268a9f14` [REVERTME] dm: script: disable xHCI runtime PM to WA USB role switch hang issue
- :acrn-commit:`ffcf6298` dm: rpmb: DM customized changes for RPMB mux kernel module
- :acrn-commit:`193da971` tools: acrnd: Refine log msg to avoid confusing.
- :acrn-commit:`1c7d2f65` vuart: change irq from 4 to 6
- :acrn-commit:`6485666a` Revert "hv: x2apic support for acrn"
- :acrn-commit:`85ececd2` hv:Simplify for-loop when walk through the vcpu
- :acrn-commit:`813e3abc` doc: Update contrib doc with Tracked-On
- :acrn-commit:`30c29015` Documentation: typo in Ubuntu tutorial and additional note
- :acrn-commit:`fabe6072` hv:Replace dynamic memory with static for microcode
- :acrn-commit:`de10df26` DM: add MSI and INTR support for i6300esb watchdog
- :acrn-commit:`25719db8` HV: move DRHD data to platform acpi info
- :acrn-commit:`ca65e8c7` HV: refine APIC base address to platform acpi info
- :acrn-commit:`8f701b0f` HV: move NR_IOAPICS to platform acpi info
- :acrn-commit:`bf834072` HV: platform acpi info refactor
- :acrn-commit:`4ed87f90` Documentation: add note and instructions for Ubuntu 16.04
- :acrn-commit:`2b449680` Documentation: minor update to the tutorial about Ubuntu as SOS
- :acrn-commit:`21458bdd` dm: storage: banned functions replace
- :acrn-commit:`e1dab512` dm: add string convert API
- :acrn-commit:`4620b935` fix "use of single line comments(s) //"
- :acrn-commit:`75b03bef` dm: add io port 0xF4 writing to force DM exit
- :acrn-commit:`9f764264` dm: add elf loader to dm
- :acrn-commit:`0e897c0a` DM: use acrn_timer api to emulate rtc
- :acrn-commit:`8fdea84a` DM: use acrn_timer api to emulate wdt
- :acrn-commit:`6ffa1aa3` DM: add acrn_timer api for timer emulation
- :acrn-commit:`d9df6e93` HV: parse seed from ABL
- :acrn-commit:`a98dd9e3` HV: trusty: set cse_svn when derive dvseed for trusty
- :acrn-commit:`102f5a01` hv: fix potential buffer overflow in vioapic.c
- :acrn-commit:`eb328d78` hv: retain rip if the fault is injected to guest
- :acrn-commit:`348e2ba1` hv: x2apic support for acrn
- :acrn-commit:`a0fb1c4c` hypervisor: Makefile: let OBJS target depend on VERSION file
- :acrn-commit:`c6c1e42b` HV:fix 'missing for discarded return value' violations
- :acrn-commit:`19e0bed5` script: re-enable audio passthru
- :acrn-commit:`eb97b2f0` tools: acrn-manager: remove assumption of fd num less than 1024
- :acrn-commit:`f582757d` tools: acrn-manager: fix fd leaking
- :acrn-commit:`dc05ffff` dm: uart: fix acrn-dm crash issue
- :acrn-commit:`e7b63aec` doc: add static core partitioning doc
- :acrn-commit:`96412ac1` hv: add suffix(U/UL) to come up MISRA-C into include
- :acrn-commit:`909d1576` dm: cleanup the cmd options for acrn-dm
- :acrn-commit:`2202b7f5` dm: virtio: reject requests that violate the virtio-block spec
- :acrn-commit:`ba4e72bd` dm: virtio: add debugging information in virtio-blk
- :acrn-commit:`7101ce87` dm: storage: remove GEOM support
- :acrn-commit:`b4a7a1ea` HV: allow no IRR when pending bit set if no APIC-V
- :acrn-commit:`38d5df72` hv:enable APICv-Posted Interrupt
- :acrn-commit:`a028567b` vpic: change assert/deassert method
- :acrn-commit:`f9a16395` dm: passthru: fix hardcoded nhlt table length
- :acrn-commit:`1d725c89` hv:Replace dynamic memory with static for vcpu
- :acrn-commit:`7dd35cb7` hv: Fix identifier reuse
- :acrn-commit:`dbd9ab07` hv: Cleanup: Remove dead code.
- :acrn-commit:`b1ccde55` hv: Cleanup: set vcpu mode in vcpu_set_regs
- :acrn-commit:`29190ed2` dm: add call to set BSP init state for UOS S3 and system reset
- :acrn-commit:`113adea0` hv: not start vm automatically when reset vm
- :acrn-commit:`b454a067` hv: remove the vm loader for UOS in hv.
- :acrn-commit:`fc575460` dm: update the bzimage loader
- :acrn-commit:`96d99954` dm: update the vsbl loader
- :acrn-commit:`853b1c74` dm: add API to set vcpu regs of guest
- :acrn-commit:`3cfbc004` hv: add hypercall to set vcpu init state
- :acrn-commit:`66b53f82` kconfig patch
- :acrn-commit:`d859182d` customize function to generate config.h with proper suffixes
- :acrn-commit:`8ccaf3c3` use genld.sh to generate link_ram.ld
- :acrn-commit:`203016b4` dm: passthru: correct the name of xdci dsdt write function
- :acrn-commit:`7f2b9a1c` hv: virq: update apicv irr/rvi before handle vmcs event injection
- :acrn-commit:`90eca21d` hv: simplify the function init_guest_state
- :acrn-commit:`a5fc3e5e` hv: Add function to set UOS BSP init state
- :acrn-commit:`08c13a9e` hv: Update SOS BSP to use new API to init BSP state
- :acrn-commit:`26627bd1` hv: add function to set AP entry
- :acrn-commit:`f7b11c83` hv: add function to reset vcpu registers
- :acrn-commit:`b2dc13d7` dm: virtio: use the correct register size
- :acrn-commit:`790d8a5c` hv:Remove CONFIG_VM0_DESC
- :acrn-commit:`3c575325` dm: passthru: add deinit_msix_table
- :acrn-commit:`244bce75` dm: passthru: enable pba emulation for msix
- :acrn-commit:`57abc88b` script: re-enable PVMMIO ppgtt update optimization for GVT-g
- :acrn-commit:`9114fbb3` Revert "DM: Disable plane_restriction on 4.19 kernel"
- :acrn-commit:`c3ebd6f3` HV: get tss address from per cpu data
- :acrn-commit:`0c7e59f0` hv: fix NULL pointer dereference in "hcall_set_vm_memory_regions()"
- :acrn-commit:`e913f9e6` dm: mevent: add edge triggered events.
- :acrn-commit:`f649beeb` dm: mevent: implement enable/disable functions
- :acrn-commit:`018aba94` dm: mevent: remove useless vmname global variable
- :acrn-commit:`4f1d3c04` dm: inline functions defined in header must be static
- :acrn-commit:`0317cfb2` hv: fix 'No brackets to then/else'
- :acrn-commit:`71927f3c` vuart: assert COM1_IRQ based on its pin's polarity
- :acrn-commit:`a11a10fa` HV:MM:gpa2hpa related error checking fix
- :acrn-commit:`041bd594` hv: improve the readability of ept_cap_detect
- :acrn-commit:`bacfc9b2` dm: fix use of uninitialized variable in monitor.c
- :acrn-commit:`6793eb06` dm: fix assertion in pci_irq_reserve
- :acrn-commit:`e0728f4b` DM USB: xHCI: fix a crash issue when usb device is disconnected
- :acrn-commit:`2b53acb5` HV:change the return type of sbuf_get and sbuf_put
- :acrn-commit:`c5f4c510` HV:fix type related violations
- :acrn-commit:`723c22fc` HV:fix expression is not boolean
- :acrn-commit:`25db6b79` IOC Mediator: Replace strtok with strsep
- :acrn-commit:`69edccc0` IOC Mediator: Add return value check for snprintf
- :acrn-commit:`cc89e52d` hv: mmu: make page table operation no fault
- :acrn-commit:`1e084b08` hv: mmu: invalidate cached translation information for guest
- :acrn-commit:`2b24b378` hv: mmu: add some API for guest page mode check
- :acrn-commit:`9fd87812` IOC Mediator: fix multi-signal parsing issue
- :acrn-commit:`b1b3f76d` dm: virtio: use strnlen instead of strlen
- :acrn-commit:`9bf5aafe` script: workarounds for UOS of 4.19-rc kernel
- :acrn-commit:`b5f77070` dm: vpit: add vPIT support
- :acrn-commit:`0359bd0f` dm: vpit: add PIT-related header files
- :acrn-commit:`eff2ac7a` hv: Remove vm_list
- :acrn-commit:`b8e59e16` hv:Replace dynamic memory with static for vm
- :acrn-commit:`ff3f9bd1` hv: Remove const qualifier for struct vm
- :acrn-commit:`5b28b378` hv: Fix for PARTITION_MODE compilation
- :acrn-commit:`eebccac2` hv: add suffix(U) in vmx.h to come up MISRA-C
- :acrn-commit:`8787b65f` dm: fix the issue when guest tries to disable memory range access
- :acrn-commit:`be0cde7d` Revert "dm: workaroud for DM crash when doing fastboot reboot"
- :acrn-commit:`b115546b` crashlog: deprecate acrnprobe_prepare and update Makefile
- :acrn-commit:`f3fc857f` crashlog: introducing crashlogctl
- :acrn-commit:`b1a05d17` crashlog: re-write usercrash-wrapper
- :acrn-commit:`6981a4df` crashlog: do not alter system behavior with watchdog
- :acrn-commit:`d800baf5` doc: tweak hld intro
- :acrn-commit:`1e385441` doc: reorganize HLD docs
- :acrn-commit:`8e21d5ee` doc: update genrest script for latest kconfiglib
- :acrn-commit:`1c0a0570` doc: update genrest script for latest kconfiglib
- :acrn-commit:`16575441` dm: vrtc: add memory configuration in RTC CMOS
- :acrn-commit:`373e79bb` Getting Started Guide: add instructions to disable cbc_* services
- :acrn-commit:`76987149` Getting Started Guide: minor clean-up
- :acrn-commit:`ce961e79` dm: acpi: set SCI_INT polarity to high active
- :acrn-commit:`064e5344` vuart: use pulse irq to assert COM1_IRQ
- :acrn-commit:`099203c1` ptdev: assert/deassert interrupt according to polarity
- :acrn-commit:`e49233ba` ioapic: set default polarity setting as high active
- :acrn-commit:`3b88d3c2` vioapic: add pin_state bitmap to set irq
- :acrn-commit:`ba68bd41` DM USB: xHCI: fix enumeration error after rebooting
- :acrn-commit:`4544d28e` hv: fix 'User name starts with underscore'
- :acrn-commit:`390861a0` DM: increase UOS memory size for MRB
- :acrn-commit:`39d54c87` EFI: Disable RELOC by default temporary
- :acrn-commit:`072e77e7` DM: Disable plane_restriction on 4.19 kernel
- :acrn-commit:`5a64af20` DM: Use the pass-through mode for IPU on 4.19 kernel
- :acrn-commit:`38099e4b` DM: Add the boot option to avoid loading dwc3_pci USB driver
- :acrn-commit:`c7611471` hv: modify static irq mappings into array of structure
- :acrn-commit:`1c0a3d9a` hv: Add API to set vcpu register
- :acrn-commit:`0e0dbbac` hv: Move the strcut acrn_vcpu_regs to public header file
- :acrn-commit:`572b59ff` doc: fix doxygen error in hypercall.h
- :acrn-commit:`6c9bae61` DM USB: xHCI: fix USB hub disconnection issue
- :acrn-commit:`0d4a88e6` DM USB: xHCI: change logic of binding libusb to native device
- :acrn-commit:`2d00a99a` DM USB: xHCI: refine stop endpoint logic
- :acrn-commit:`adc79137` hv: efi_context refine
- :acrn-commit:`ba1aa407` hv: add struct acrn_vcpu_regs
- :acrn-commit:`843f7721` hv: Change the struct cpu_gp_regs name to acrn_gp_regs
- :acrn-commit:`b207f1b9` hv: struct seg_desc_vmcs name change
- :acrn-commit:`5c923296` hv:clear up the usage of printf data struct
- :acrn-commit:`965f8d10` hv: fix irq leak for MSI IRQ
- :acrn-commit:`67ff326e` hv: retain the timer irq
- :acrn-commit:`07e71212` hv:Replace dynamic memory allocation for vuart
- :acrn-commit:`7ce0e6a3` hv:Clear up printf related definition
- :acrn-commit:`ed06b8a7` hv: fix 'Void procedure used in expression'
- :acrn-commit:`9a05fbea` HV: remove IRQSTATE_ASSERT/IRQSTATE_DEASSERT/IRQSTATE_PULSE
- :acrn-commit:`9df8790f` hv: Fix two minor issues in instruction emulation code
- :acrn-commit:`be0651ad` Getting Started Guide: fix highlighting in launch_uos.sh
- :acrn-commit:`37014caa` Documentation: add pointer to the documentation generation in GSG
- :acrn-commit:`7b26b348` Documentation: update list of bundles to be installed in GSG
- :acrn-commit:`f45c3bd2` Documentation: add instruction to use a specific version of Clear
- :acrn-commit:`398ac203` Update acrn_vm_ops.c
- :acrn-commit:`e6c3ea3b` tools: acrn-manager: init vmmngr_head with LIST_HEAD_INITIALIZER
- :acrn-commit:`7b0b67df` dm: virtio-net: add vhost net support
- :acrn-commit:`3fdfaa3d` dm: virtio: implement vhost chardev interfaces
- :acrn-commit:`e3f4e34c` dm: virtio: implement vhost_vq_register_eventfd
- :acrn-commit:`150ad30b` dm: virtio: implement vhost_set_mem_table
- :acrn-commit:`befbc3e9` dm: virtio: implement vhost_vq interfaces
- :acrn-commit:`bb34ffe6` dm: virtio: add vhost support
- :acrn-commit:`781e7dfb` dm: virtio: rename virtio ring structures and feature bits
- :acrn-commit:`dd6a5fbe` HV: Add hypercall to set/clear IRQ line
- :acrn-commit:`05ad6d66` hv: drop the macro arguments acting as formal parameter names
- :acrn-commit:`74622d7d` hv: merge hv_lib.h and hypervisor.h
- :acrn-commit:`3178ecea` hv: Fix the warning for ACRN release build
- :acrn-commit:`6bcfa152` hv: Enable the compiler warning as error for HV
- :acrn-commit:`2111fcff` hv: vtd: add config for bus limitation when init
- :acrn-commit:`6fcaa1ae` hv: bug fix in atomic.h
- :acrn-commit:`026ae83b` hv: include: fix 'Unused procedure parameter'
- :acrn-commit:`68ce114b` doc: add tool for verifying installed doc tools
- :acrn-commit:`c30437de` Fix Doxygen comment in hypercall.h header file
- :acrn-commit:`56992c73` dm: combine VM creating and ioreq shared page setup
- :acrn-commit:`94513ab7` dm: Add vhm ioeventfd and irqfd interfaces
- :acrn-commit:`a189be26` HV: Add one hcall to set the upcall vector passed from sos_kernel
- :acrn-commit:`22869913` HV: Add the definition of VECTOR_HYPERVISOR_CALLBACK_VHM
- :acrn-commit:`a8e688eb` HV: Use the variable to fire VHM interrupt
- :acrn-commit:`89ca54ca` hv:Fix unused var value on all paths
- :acrn-commit:`f1cce671` Makefile: fix cross-compiling issues
- :acrn-commit:`8787c06d` hv: arch: fix 'Unused procedure parameter'
- :acrn-commit:`2908f09f` hv: fix ramdump regression
- :acrn-commit:`52ee6154` tools: acrnlog: update Makefile
- :acrn-commit:`74c4d719` tools: acrnlog: fix several compiler warnings
- :acrn-commit:`c51e2139` tools: acrntrace: update Makefile
- :acrn-commit:`5e0acac4` tools: acrntrace: fix several compiler warnings
- :acrn-commit:`1b9a3b3e` tools: acrn-manager: update Makefile
- :acrn-commit:`227a8c43` tools: acrn-manager: fix warnings before updating Makefile
- :acrn-commit:`270a8332` tools: acrnd: bugfix: service lack of prerequisition
- :acrn-commit:`5affe53a` tools: acrn-crashlog: update Makefile flags
- :acrn-commit:`726711e2` tools: acrn-crashlog: fix some compiler warnings
- :acrn-commit:`4e17d207` hv: fix 'Static procedure is not explicitly called in code analysed'
- :acrn-commit:`ac9ebc5e` update to support v0.2 release
- :acrn-commit:`71b047cb` hv: fix 'Switch case not terminated with break'
- :acrn-commit:`f3758850` dm: virtio_net: remove netmap/vale backend support
- :acrn-commit:`e0973e48` hv: ioapic: convert some MACROs to inline functions
- :acrn-commit:`99ed5469` DM: add a thread to monitor UOS ptdev intr status
- :acrn-commit:`d123083f` HV: add hypercall to monitor UOS PTdev intr status
- :acrn-commit:`918403f9` HV: modify code for intr storm detect & handling
- :acrn-commit:`de68ee7a` version: 0.3-unstable

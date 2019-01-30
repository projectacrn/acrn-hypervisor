.. _release_notes_0.2:

ACRN v0.2 (Sep 2018)
####################

We are pleased to announce the release of Project ACRN version 0.2.

ACRN is a flexible, lightweight reference hypervisor, built with
real-time and safety-criticality in mind, optimized to streamline
embedded development through an open source platform. Check out the
:ref:`introduction` for more information.


All project ACRN source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, and documentation.
You can either download this source code as a zip or tar.gz file (see
the `ACRN v0.2 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v0.2>`_ or
use Git clone and checkout commands:

.. code-block:: bash

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v0.2

The project's online technical documentation is also tagged to correspond
with a specific release: generated v0.2 documents can be found at
https://projectacrn.github.io/0.2/.  Documentation for the latest
(master) branch is found at https://projectacrn.github.io/latest/.


Version 0.2 new features
************************

VT-x, VT-d
================
Based on Intel VT-x virtualization technology, ACRN emulates a virtual
CPU with core partition and simple schedule. VT-d provides hardware
support for isolating and restricting device accesses to the owner of
the partition managing the device. It allows assigning I/O devices to a
VM, and extending the protection and isolation properties of VMs for I/O
operations.

PIC/IOAPIC/MSI/MSI-X/PCI/LAPIC
================================
ACRN hypervisor supports virtualized APIC-V/EPT/IOAPIC/LAPIC
functionality.

Ethernet
================
ACRN hypervisor supports virtualized Ethernet functionality. Ethernet
Mediator is executed in the Service OS and provides packet forwarding
between the physical networking devices (Ethernet, Wi-Fi, etc.) and
virtual devices in the Guest VMs(also called "User OS"). Virtual
Ethernet device could be shared by Linux, Android, and Service OS guests
for regular (i.e. non-AVB) traffic. All hypervisor para-virtualized I/O
is implemented using the VirtIO specification Ethernet pass-through.

Storage (eMMC)
================
ACRN hypervisor supports virtualized non-volatile R/W storage for the
Service OS and Guest OS instances, supporting VM private storage and/or
storage shared between Guest OS instances.

USB (xDCI)
================
ACRN hypervisor supports virtualized assignment of all USB xHCI and/or
xDCI controllers to a Guest OS from the platform.

USB Mediator (xHCI and DRD)
===========================
ACRN hypervisor supports a virtualized USB Mediator.

CSME
================
ACRN hypervisor supports a CSME to a single Linux, Android, or RTOS
guest or the Service OS even when in a virtualized environment.

WiFi
================
ACRN hypervisor supports the passthrough assignment of the WiFi
subsystem to the IVI, enables control of the WiFi as an in-vehicle
hotspot for 3rd party devices, provides 3rd party device applications
access to the vehicle, and provides access of 3rd party devices to the
TCU provided connectivity.

IPU (MIPI-CS2, HDMI-in)
========================
ACRN hypervisor supports passthrough IPU assignment to Service OS or
guest OS, without sharing.

Bluetooth
================
ACRN hypervisor supports bluetooth controller passthrough to a single
Guest OS (IVI).

GPU  – Preemption
==================
GPU Preemption is one typical automotive use case which requires the
system to preempt GPU resources occupied by lower priority workloads.
This is done to ensure performance of the most critical workload can be
achieved. Three different schedulers for the GPU are involved: i915 UOS
scheduler, Mediator GVT scheduler, and i915 SOS scheduler.

GPU – display surface sharing via Hyper DMA
============================================
Surface sharing is one typical automotive use case which requires
that the SOS accesses an individual surface or a set of surfaces
from the UOS without having to access the entire frame buffer of
the UOS. Hyper DMA Buffer sharing extends the Linux DMA buffer
sharing mechanism where one driver is able to share its pages
with another driver within one domain.

S3
================
ACRN hypervisor supports S3 feature, partially enabled in LaaG.


Fixed Issues
************

* :acrn-issue:`663` - Black screen displayed after booting SOS/UOS
* :acrn-issue:`676` - Hypervisor and DM version numbers incorrect
* :acrn-issue:`1126` - VPCI coding style and bugs fixes for partition mode
* :acrn-issue:`1125` - VPCI coding style and bugs fixes found in integration testing for partition mode
* :acrn-issue:`1101` - missing acrn_mngr.h
* :acrn-issue:`1071` - hypervisor cannot boot on skylake i5-6500
* :acrn-issue:`1003` - CPU: cpu info is not correct
* :acrn-issue:`971` -  acrncrashlog functions need to be enhance
* :acrn-issue:`843` - ACRN boot failure
* :acrn-issue:`721` - DM for IPU mediation
* :acrn-issue:`707` - Issues found with instructions for using Ubuntu as SOS
* :acrn-issue:`706` - Invisible mouse cursor in UOS
* :acrn-issue:`424` - Clear Linux OS desktop GUI of SOS fails to launch


Known Issues
************
:acrn-issue:`677` - SSD Disk ID not consistent between SOS/UOS
   The SSD disk ID in the UOS is not the same as in the SOS when the SSD
   device is passed-through to the UOS (it should be). The ID is also
   changing after a reboot (it shouldn't).  **Impact:** There is no impact
   to functionality.  **Workaround:** None. The issues will be fixed in the
   next release.


:acrn-issue:`1268` - GPU hangs when running GfxBench Car Chase in SOS and UOS.
   A "GPU HANG" log messages is printed in the dmesg log in SOS and UOS and
   is fails to load GfxBench Car Chase animation in HDMI and VGA monitors.
   **Impact:** Car Chase may stop running after the GPU hangs, but does not
   block other Gfx apps.  **Workaround:** None. The issue will be fixed in
   the next release.


:acrn-issue:`1270` - SOS and UOS play video but don't display video animation output on monitor.
   Video does not display in SOS and UOS. Animation is not displayed with
   the running timer. **Impact:** There is no video animation on monitor
   screen when playing video in SOS or UOS.  **Workaround:** None. The
   issue will be fixed in the next release.


:acrn-issue:`1319` - SD card pass-through: UOS can’t see SD card after UOS reboot.
   SD card could not be found after UOS reboot in pass-through mode.
   **Impact:** There is no SD card after UOS reboot.
   **Workaround:** None. The issue will be fixed in the next release.


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

These commits have been added to the acrn-hypervisor repo since the v0.1
release in July 2018 (click on the CommitID link to see details):

.. comment

   This list is obtained from the command:
   git log --pretty=format:'- :acrn-commit:`%h` %s' --after="2018-03-01"

- :acrn-commit:`7dd3207` doc: fix typo in network virt hld
- :acrn-commit:`01ce3b6` doc: sort title in developer HLD section
- :acrn-commit:`dae98a0` doc: add network virtualization hld
- :acrn-commit:`55a71e4` doc: add watchdog high-level design
- :acrn-commit:`31285a9` doc: add security hld document
- :acrn-commit:`8c9e0d1` hv: init_iommu rework to enable IOMMU for partition mode ACRN
- :acrn-commit:`5373190` dm: passthru: enable NHLT table for audio passthru
- :acrn-commit:`33acca5` tools: acrn-crashlog: exclude crashlog tool for release version
- :acrn-commit:`9817eb3` Add a comment for switch fallthrough to quiet lint warning
- :acrn-commit:`0306bb4` Removed dead funcs in EFI stub module
- :acrn-commit:`1d15b98` Replace the call to emalloc() to uefi pool allocation
- :acrn-commit:`951a24c` allocate boot related struct right after hypervisor memory
- :acrn-commit:`6085781` Replace __emalloc() with a call to uefi allocate_page()
- :acrn-commit:`fea102e` Remove emalloc_for_low_mem() routine in EFI boot code of HV
- :acrn-commit:`ccf5624` hv:irq: avoid out-of-range access to irq_alloc_bitmap[]
- :acrn-commit:`4a038d1` dm: workaround for DM crash when doing fastboot reboot
- :acrn-commit:`688cdda` DM USB: xHCI: enable USB xHCI emulation in LaaG and AaaG.
- :acrn-commit:`d4afddc` Revert "hv: init_iommu rework to enable IOMMU for partition mode ACRN"
- :acrn-commit:`40dfd3f` doc: edit 0.2 release notes
- :acrn-commit:`737c29f` doc: Add known issues in v0.2 release note.
- :acrn-commit:`994a375` HV:fix potential buffer overflow issues
- :acrn-commit:`b501ce4` HV: remove dead APIC info MACROs in bsp
- :acrn-commit:`2197f43` HV: Add acpi_fixup api to override acpi on needs
- :acrn-commit:`0d5ad8a` HV: add simple parser for ACPI data table
- :acrn-commit:`a1e9fdc` HV: add Kconfig of CONSTANT_ACPI
- :acrn-commit:`eb8c4fb` hv:Fix Implicit conversion:actual to formal param
- :acrn-commit:`8f0cb56` HV: trusty: refine version checking when initializing trusty
- :acrn-commit:`9d60220` hv: remove unused MACROs
- :acrn-commit:`bb0a2bc` HV:Hypercall:Remove redundant error checking
- :acrn-commit:`d8508e4` hv: init_iommu rework to enable IOMMU for partition mode ACRN
- :acrn-commit:`2af08d9` HV: refine 'struct lapic_regs' definition.
- :acrn-commit:`5329ced` hv: vtd: fix potential buffer overflow in suspend/resume
- :acrn-commit:`f355cdf` Revert "DM USB: xHCI: enable USB xHCI emulation in LaaG and AaaG."
- :acrn-commit:`83d1ddc` hv:Delete the dead code
- :acrn-commit:`c307e1b` hv: long and long long is same for printf within x86_64
- :acrn-commit:`a47f5d4` doc: fix Makefile to address multiple publishers
- :acrn-commit:`41a1714` doc: fix spaces in release notes
- :acrn-commit:`3c8157b` doc: Add new feature in v0.2 release note
- :acrn-commit:`c03ea2d` DM USB: xHCI: enable USB xHCI emulation in LaaG and AaaG.
- :acrn-commit:`b4755cd` DM USB: xHCI: enable 'cold plug' mode
- :acrn-commit:`612037e` DM USB: xHCI: enable Flat Mode Hub emulation support.
- :acrn-commit:`d886375` hv: clean up spinlock
- :acrn-commit:`8858634` HV: update opcode when decode_two_byte_opcode()
- :acrn-commit:`5023937` hv: merge pgtable_types.h and pgtable.h
- :acrn-commit:`83e7995` hv: clean up some header files
- :acrn-commit:`5a996ce` hv:irq: correct the size of irq_alloc_bitmap
- :acrn-commit:`4fd5102` hv:treewide:fix multiple MISRAC violations
- :acrn-commit:`00edd83` hypercall: no need check HV memory overlap for MR_DEL
- :acrn-commit:`876cc68` tools:acrn-crashlog: Change the algorithm of generating event key
- :acrn-commit:`b1ba12a` hv: clean up spinlock wrappers
- :acrn-commit:`44a2d77` hv: add SMAP/SMEP check during guest page walking
- :acrn-commit:`d958d31` hv: fix the issue of movs emulation
- :acrn-commit:`d84f7a4` hv: clean up udelay/mdelay related code
- :acrn-commit:`7cab77d` hv: clean up div related code
- :acrn-commit:`1d2ed1a` hv: vioapic/vpic: clean up spinlock wrappers
- :acrn-commit:`4f20c44` dm: passthru: fix a bug in msix read/write
- :acrn-commit:`60c05ac` hv:Replace vlapic pointer with instance in vcpu_arch
- :acrn-commit:`f744762` hv:Remove redundancy 'vlapic' in 'struct vcpu'
- :acrn-commit:`aae5018` hv:Move vlapic structure to vlapic.h
- :acrn-commit:`fdb6409` hv:Replace dynamic memory allocation for apic access address
- :acrn-commit:`887ebf0` hv: Replace dynamic memory allocation for MSR bitmap
- :acrn-commit:`02e7edc` hv: Replace dynamic memory allocation for I/O bitmaps
- :acrn-commit:`eada04b` hv:Replace dynamic memory allocation for vmcs region
- :acrn-commit:`ca75d50` IOC mediator: add RTC feature
- :acrn-commit:`42d9b24` doc: allow overriding displayed doc version
- :acrn-commit:`dbcbe7d` HV: change wake vector to accommodate abl 1820HF1release
- :acrn-commit:`bca43b5` hv: avoid memory leak in trampoline code preparing
- :acrn-commit:`9e76cf4` doc: Add fixed issues in v0.2 release note
- :acrn-commit:`f1e87f6` dm: vrtc: use signalfd to poll signal from timer
- :acrn-commit:`bcaede0` hv: treewide: fix 'Use of function like macro'
- :acrn-commit:`d72e65c` trusty: do not destroy secure world if it's not created
- :acrn-commit:`8773dfb` vlapic: unmap vlapic base only for SOS
- :acrn-commit:`457ac74` vcpu: replace start_vcpu with run_vcpu
- :acrn-commit:`2978c01` io: tiny fix for error message
- :acrn-commit:`bfcf546` Doc: add interrupt storm mitigation explanation.
- :acrn-commit:`d8c4619` HV: change wake vector info to accommodate abl
- :acrn-commit:`4ae88bb` tools: acrn-manager: fix acrnctl reset issue
- :acrn-commit:`f42209c` tools: acrn-manager: remove unnecessary "current" field
- :acrn-commit:`0ca90ba` tools: acrn-manager: rework acrnd resume flow
- :acrn-commit:`26b8b3b` tool: acrn-manager: do not wakeup SOS in advance
- :acrn-commit:`c6b7940` samples: Add AliOS as guest launch option
- :acrn-commit:`a7de5a1` samples: Add tap name as launch function parameter
- :acrn-commit:`bcfe447` DM: deinit initialized pci device when failed
- :acrn-commit:`99285f8` HV: improve pass-thru device interrupt process
- :acrn-commit:`b4e03f2` hv: virq: make irq_window_enabled useful
- :acrn-commit:`8e29615` hv: apicv: enable interrupt-window if any pending external interrupts
- :acrn-commit:`46c3276` hv: apicv: avoid enable interrupt window if interrupt delivery enabled
- :acrn-commit:`f5ca189` dm: bios: update vsbl to v0.9
- :acrn-commit:`047f4e9` Documentation: update to AcrnGT official name
- :acrn-commit:`97aeb7f` hv: pgtable: fix 'Use of function like macro'
- :acrn-commit:`6ee9321` security: Enable '-fpie -pie' options
- :acrn-commit:`5c5aed6` hv:Change several VMX APIs to void type
- :acrn-commit:`e4e38e1` hv:Check if VMX capability is locked with incorrect value
- :acrn-commit:`6593080` hv: Replace dynamic allocation with static memory for vmxon_region
- :acrn-commit:`4360235` hv: treewide: fix 'Macro parameter not in brackets'
- :acrn-commit:`30b77ab` DM: unmap ptdev BAR when deinit
- :acrn-commit:`1b334ec` hv: replace 'return' with 'panic' in bsp_boot_post
- :acrn-commit:`bad8d81` IOC mediator: add new signal for VBUS control
- :acrn-commit:`f2f719c` hv: fix 'Procedure is not called or referenced in code analyzed'
- :acrn-commit:`3718177` hv:Replace dynamic allocation with static memory for shell
- :acrn-commit:`c045442` DM: watchdog: correct 2 MACRO define
- :acrn-commit:`198c6e9` DM: coding style: replace tab with space
- :acrn-commit:`d32ef9b` doc: fix doc misspellings
- :acrn-commit:`5103002` doc: prepare for versioned release notes
- :acrn-commit:`5c3e4d1` tools: acrntrace: fix a variable uninitialized issue
- :acrn-commit:`56f2c1a` tools: acrn-crashlog: correct usercrash-wrapper path
- :acrn-commit:`6703879` hv: treewide: convert some MACROs to inline functions
- :acrn-commit:`37fd387` tools: acrn-crashlog: add usercrash_c in the pipe of core_pattern
- :acrn-commit:`a4cb391` hv: fixup format of log message in vm_load.c
- :acrn-commit:`96809c4` DM NPK: enable the NPK virtualization for AaaG
- :acrn-commit:`9a27659` DM NPK: use a slice (8 masters) as the minimal unit for NPK virt
- :acrn-commit:`d8c97c1` hv: fix broken relocation feature
- :acrn-commit:`36c4a27` HV: Fix VPCI bugs found in integration testing for partition mode
- :acrn-commit:`308910e` HV: Updated vm description table for partition mode
- :acrn-commit:`c9ea890` HV: VPCI coding style fix
- :acrn-commit:`54439ec` hv: treewide: fix 'Expression is not Boolean'
- :acrn-commit:`f611012` HV: Refine two log info about vcpu and instr_emul
- :acrn-commit:`96dba0d` hv: fix MISRA-C issues related to space or newline
- :acrn-commit:`d67eefb` hv: mmu: use get/set_pgentry to get/set page table entry
- :acrn-commit:`7f9befb` hv: ept: remove find_next_table
- :acrn-commit:`9257ecf` hv: mmu: cleanup mmu.h
- :acrn-commit:`06ab2b8` hv: mmu: add 1GB page capability check when CPU boot
- :acrn-commit:`58fffcd` hv: mmu: rename PTT_HOST to PTT_PRIMARY
- :acrn-commit:`c102c44` hv: Device MSIs in partition mode ACRN
- :acrn-commit:`ef1a730` Documentation: correct URL pointing at sample 'acrn.conf' file
- :acrn-commit:`25dacc5` security: Enable '-fpie, -pie' options
- :acrn-commit:`10c64a5` hv: fix MISRA-C issues related to for loop
- :acrn-commit:`852f613` samples: remove extra option for dm
- :acrn-commit:`dba52ba` IOC mediator: fix no CBC signals after resuming
- :acrn-commit:`43741ba` hv: Leave interrupts disabled during vmexit - ACRN partition mode
- :acrn-commit:`348422d` doc: fix graphviz scanning and processing
- :acrn-commit:`e49c42d` Documentation: update GVT-G-porting-image1.png for AcrnGT official name
- :acrn-commit:`a8ac452` dm: cmos: move cmos storage out of vmctx
- :acrn-commit:`fa7eb1f` tools:acrn-crashlog: Document of configuration file
- :acrn-commit:`12c1687` hv:No need to create inverted page tables for trusty memory
- :acrn-commit:`2a184f3` hv: code clean up regarding to guest_msrs
- :acrn-commit:`947e86d` HV: restore correct gpa for guest normal world
- :acrn-commit:`da4c95b` tools: acrn-manager: fix several warnings
- :acrn-commit:`4e8798e` hv:Replace vioapic pointer with instance in structure vm
- :acrn-commit:`29dbd10` hv:Replace vuart pointer with instance in structure vm
- :acrn-commit:`0b54946` hv:Replace vpic pointer with instance in structure vm
- :acrn-commit:`de53964` HV: Removed the unused parameters and union from gdt
- :acrn-commit:`8d35f4e` HV: wrap and enable hkdf_sha256 key derivation based on mbedtls
- :acrn-commit:`12aa2a4` HV: crypto lib code clean up
- :acrn-commit:`71577f6` HV: extract hkdf key derivation files from mbedtls
- :acrn-commit:`925503c` hv: Build fix - ACRN partition mode
- :acrn-commit:`c5dcb34` DM USB: xHCI: fix a potential issue of crash
- :acrn-commit:`7bc1a3f` HV: Refine APICv capabilities detection
- :acrn-commit:`f95d07d` hv: vtd: use EPT as translation table for PTDev in SOS
- :acrn-commit:`4579e57` hv: add gva check for the case gva is from instruction decode
- :acrn-commit:`7dde0df` hv: add GVA validation for MOVS
- :acrn-commit:`b01a812` hv: add new function to get gva for MOVS/STO instruction
- :acrn-commit:`8480c98` hv: move check out of vie_calculate_gla
- :acrn-commit:`54c2541` hv: remove unnecessary check for gva
- :acrn-commit:`5663dd7` hv: extend the decode_modrm
- :acrn-commit:`3b6ccf0` HV: remove callbacks registration for APICv functions
- :acrn-commit:`93c1b07` hv: mmu: remove old map_mem
- :acrn-commit:`f3b825d` hv: trusty: use ept_mr_add to add memory region
- :acrn-commit:`4bb8456` hv: ept: refine ept_mr_add base on mmu_add
- :acrn-commit:`da57284` hv: ptdev: simplify struct ptdev_msi_info
- :acrn-commit:`2371839` hv: ptdev: remove vector index from structure ptdev_msi_info
- :acrn-commit:`d8cc29b` hv: ptdev: check whether phys_pin is valid in add_intx_remapping
- :acrn-commit:`e8c0763` hv: ptdev: add source_id for ptdev to identify source
- :acrn-commit:`6367650` hv: debug: add the hypervisor NPK log
- :acrn-commit:`3c6df9b` hv: add mmio functions for 64bit values
- :acrn-commit:`dcae438` hv: add a hypercall for the hypervisor NPK log
- :acrn-commit:`f4eef97` hv: ptdev: simplify ptdev_intx_pin_remap logic
- :acrn-commit:`a6c2065` hv: apicv: change the name of vapic to apicv
- :acrn-commit:`a0c625b` hv: apicv: change the apicv related API with vlapic_apicv prefix
- :acrn-commit:`74ff712` hv: vlapic: local APIC ID related code cleaning up
- :acrn-commit:`c43d0e4` hv:Changed several APIs to void type
- :acrn-commit:`b75a7df` hv: vcpuid: disable some features in cpuid
- :acrn-commit:`42aaf5d` hv: code clean up regarding to % and / operations
- :acrn-commit:`0c630d9` dm: cmos: fix a logic error for read to clear range
- :acrn-commit:`3e598eb` hv: fix 'No definition in system for prototyped procedure'
- :acrn-commit:`65e01a0` hv: pirq: use a bitmap to maintain irq use status
- :acrn-commit:`e0d40fe` HV:refine 'apic_page' & 'pir_desc' in 'struct acrn_vlapic'
- :acrn-commit:`17ef507` ipu: virtio-ipu4 as default IPU DM
- :acrn-commit:`8924f6d` hv: vmx: fix 'Array has no bounds specified'
- :acrn-commit:`6988a17` DM USB: xHCI: Change the default USB xHCI support to pass through.
- :acrn-commit:`1017d91` hv: treewide: fix 'Empty parameter list to procedure/function'
- :acrn-commit:`7a4dcfc` hv: treewide: fix 'Function prototype/defn param type mismatch'
- :acrn-commit:`752e311` hv:fixed MISRA-C return value violations
- :acrn-commit:`431ef57` hv: vioapic: fix 'No definition in system for prototyped procedure'
- :acrn-commit:`b17de6a` hv: Support HV console for multiple VMs - ACRN partition mode
- :acrn-commit:`b8c1fd6` dm: pass vrpmb key via cmos interface
- :acrn-commit:`c8c0e10` HV: enlarge the CMA size for uos trusty
- :acrn-commit:`40fd889` hv:fixed several return value violations
- :acrn-commit:`b37008d` HV: check secure/normal world for EPTP in gpa2hpa
- :acrn-commit:`10a4c6c` samples: let nuc uos only start with 1 cpu
- :acrn-commit:`709cd57` hv: lib: add ffz64_ex
- :acrn-commit:`5381738` hv: pirq: change the order of functions within irq.c
- :acrn-commit:`a8cd692` hv: pirq: clean up irq handlers
- :acrn-commit:`2c044e0` hv: pirq: refactor vector allocation/free
- :acrn-commit:`1bf2fc3` hv: pirq: refactor irq num alloc/free
- :acrn-commit:`f77d885` hv: pirq: clean up unnecessary fields of irq_desc
- :acrn-commit:`bdcc3ae` hv: fixed compiling warning
- :acrn-commit:`40745d9` hv: vuart: fix the data type violations
- :acrn-commit:`d82a86e` DM USB: xHCI: enable USB xHCI emulation in LaaG and AaaG.
- :acrn-commit:`150b389` hv: fix size issue in mptable guest copy - ACRN partition mode
- :acrn-commit:`0c93a13` hv: sw_loader for VMs in ACRN partition mode
- :acrn-commit:`fce5862` hv: vm_description fix for partition ACRN
- :acrn-commit:`38a1898` hv: Fix comments referring to wrong hypervisor name
- :acrn-commit:`d3db5a6` HV: Add const qualifiers where required
- :acrn-commit:`e280d95` hv: vmx_vapic: fix two build warnings
- :acrn-commit:`39b4fec` hv: apicv: explicit log for SMI IPI unsupported
- :acrn-commit:`604b5a4` hv: apicv: remove APIC_OFFSET_SELF_IPI(0x3F0) register
- :acrn-commit:`93f9126` hv: apicv: remove x2apic related code
- :acrn-commit:`8d38318` hv: virq: disable interrupt-window exiting in vmexit handler
- :acrn-commit:`f4513f9` update to fix format issue of ReST
- :acrn-commit:`5a6ee3f` update doc -Using Ubuntu as the Service OS
- :acrn-commit:`4ecbdf0` tools: acrn-crashlog: update core_pattern content conditionally
- :acrn-commit:`8ff0efc` update user name cl_sos
- :acrn-commit:`99e8997` DM: Add boot option of "i915.enable_guc=0" to disable Guc on UOS new kernel
- :acrn-commit:`36d5fdb` DM/Samples: Add the boot option of "i915.enable_guc=0" to disable guc on SOS new kernel
- :acrn-commit:`5b8c7a5` hv: VM BSP vcpu mode for ACRN partition mode
- :acrn-commit:`c234acb` fix spec_ctrl msr save/restore
- :acrn-commit:`022ef92` hv: Add vrtc emulation support for ACRN partition mode
- :acrn-commit:`f63c7a7` dm: virtio: set VBS-K status to VIRTIO_DEV_INIT_SUCCESS after reset
- :acrn-commit:`1378a84` dm: virtio: add support for VBS-K device reset
- :acrn-commit:`16a8174` hv: vioapic: bug fix update PTDEV RTE
- :acrn-commit:`101ab60` hv: Build fix for Partition mode
- :acrn-commit:`d030595` HV: remove 'spinlock_rfags' declaration
- :acrn-commit:`932bc32` DM: virtio rpmb backend driver updates
- :acrn-commit:`3df3c9f` hv: vuart: fix 'Shifting value too far'
- :acrn-commit:`de487ff` hv:fix return value violations for vpic/vioapic
- :acrn-commit:`cad8492` enable weston to fix: #663
- :acrn-commit:`f2a3e1f` quick fix: fix build failure for release version
- :acrn-commit:`bb5377b` HV: change wake vector info to accommodate ww32 sbl
- :acrn-commit:`f8f49d4` dump vcpu registers on correct vcpu
- :acrn-commit:`4b03c97` add smp_call_function support
- :acrn-commit:`8ef0721` idle: enable IRQ in default idle
- :acrn-commit:`e19d36f` change pcpu_sync_sleep to wait_sync_change
- :acrn-commit:`49d3446` lapic: add send_dest_ipi function
- :acrn-commit:`6e96243` HV: io: drop REQ_STATE_FAILED
- :acrn-commit:`ca83c09` hv: treewide: fix multiple MISRAC violations
- :acrn-commit:`0292e14` DM USB: xHCI: enable xHCI SOS S3 support
- :acrn-commit:`0b405ee` DM USB: xHCI: change flow of creation of virtual USB device
- :acrn-commit:`b359dc3` DM USB: xHCI: code cleanup: change variable name
- :acrn-commit:`27eeea4` DM USB: xHCI: refine port assignment logic
- :acrn-commit:`5cc389a` DM USB: xHCI: limit bus and port numbers of xHCI
- :acrn-commit:`2abec44` DM USB: introduce struct usb_native_devinfo
- :acrn-commit:`363b4da` DM USB: xHCI: refine xHCI PORTSC Register related functions
- :acrn-commit:`b746377` DM USB: xHCI: fix an xHCI issue to enable UOS s3 feature
- :acrn-commit:`b5a233d` HV: Enclose debug specific code with #ifdef HV_DEBUG
- :acrn-commit:`b086162` dm: monitor: bugfix: update wakeup reason before call resume() callback
- :acrn-commit:`a86a25f` tools: acrnd: Fixed get_sos_wakeup_reason()
- :acrn-commit:`2d802d0` tools: vm_resume() requires wakeup reason
- :acrn-commit:`64a9b2b` Revert "[REVERT-ME]: disable turbo mode"
- :acrn-commit:`18d44cc` tools: acrnalyze: Make the result easier to read
- :acrn-commit:`08dd698` hv: pirq: rename common irq APIs
- :acrn-commit:`8fda0d8` hv: pirq: add static irq:vector mappings
- :acrn-commit:`f6e45c9` hv: pirq: remove unnecessary dev_handler_node struct
- :acrn-commit:`d773df9` hv: pirq: remove support of physical irq sharing
- :acrn-commit:`6744a17` hv: treewide: fix 'Shifting value too far'
- :acrn-commit:`a9151ff` hv: add compile time assert for static checks
- :acrn-commit:`69522dc` hv: move boot_ctx offset definitions
- :acrn-commit:`197706f` HV: Use the CPUID(0x16) to obtain tsc_hz when zero tsc_hz is returned by 0x15 cpuid
- :acrn-commit:`7d83abb` HV: Add the emulation of CPUID with 0x16 leaf
- :acrn-commit:`e0eeb8a` HV: Limit the CPUID with >= 0x15 leaf
- :acrn-commit:`d5d3d2d` tools: acrnlog: Add [-t interval] [-h] to usage
- :acrn-commit:`a9a2f91` tools: acrntrace: Remove unused parameters "-r" related things
- :acrn-commit:`76e43ac` HV: handle trusty on vm reset
- :acrn-commit:`c55b696` HV: remove 'warm_reboot()'function and other minor cleanup
- :acrn-commit:`77011ce` HV: Merge hypervisor debug header files
- :acrn-commit:`a6bc36f` HV: refine shell.c & shell_priv.h
- :acrn-commit:`28c8923` HV: rename 'shell_internal.h' to 'shell_priv.h'
- :acrn-commit:`2fbf707` HV: Logical conjunction needs brackets
- :acrn-commit:`6f1c5fa` HV: Logical conjunction needs brackets under /arch/x86/guest
- :acrn-commit:`7a739cc` DM: Add dm for IPU mediation
- :acrn-commit:`a568c9e` dm: bios: update vsbl to v0.8.1
- :acrn-commit:`5a559ce` fixed cpu info incorrect and remove 2M hugepages
- :acrn-commit:`f11b263` remove 2M hugepages
- :acrn-commit:`462284f` HV: add pcpu id check before send IPI
- :acrn-commit:`c25a62e` hv: Create E820 entries for OS in partitioning mode ACRN
- :acrn-commit:`ab29614` HV: VMX reshuffle: put EPT check before enabling
- :acrn-commit:`112b4ea` hv: Fixing build issue with PARTITION_MODE
- :acrn-commit:`7380c16` hv: Add vuart flag to VM descriptions in partition mode
- :acrn-commit:`9e02ef5` hv: Partition mode ACRN -kernel load and bootargs load address
- :acrn-commit:`4e99afc` hv: treewide: fix 'Empty parameter list to procedure/function'
- :acrn-commit:`fc2701d` HV: move vioapic.c & vpic.c to 'dm' folder
- :acrn-commit:`8348800` dm: virtio_rnd: use delayed blocking IO to make virtio_rnd works on Linux based SOS
- :acrn-commit:`98aa74b` hv: treewide: fix 'No default case in switch statement'
- :acrn-commit:`2a65681` misc: totally remove misc folder
- :acrn-commit:`49322ac` dm: storage: support cache mode toggling
- :acrn-commit:`f4fcf5d` dm: virtio: remove hv_caps from virtio_ops
- :acrn-commit:`a2b2991` doc: update virtio-blk usage in HLD
- :acrn-commit:`2592ea8` dm: storage: support writethru and writeback mode
- :acrn-commit:`42cabf6` hv: Handling IO exits in ACRN for partition mode
- :acrn-commit:`a8fcc0f` HV: Add vm_id entry to VM description in partitioning mode
- :acrn-commit:`d0e9f24` hv: Interrupt handling in ACRN partition mode
- :acrn-commit:`0c88f9b` hv: Build mptable for OS in partition mode
- :acrn-commit:`e40b998` hv: Add EPT mapping for UOS in partitioning mode
- :acrn-commit:`c492a14` hv: pirq: do not indicate priority when allocate vector
- :acrn-commit:`229bf32` hv:Refine destroy_secure_world API
- :acrn-commit:`40196d1` hv: treewide: fix 'inline function should be declared static'
- :acrn-commit:`cdd19dc` hv: treewide: fix 'Variable should be declared static'
- :acrn-commit:`183ca5d` HV: Adding hostbridge vdev device support for partition hypervisor
- :acrn-commit:`181de19` HV: Adding passthru vdev device support for partition hypervisor
- :acrn-commit:`5f3ea06` HV: Implementing PCI CFG vm-exit handler for partition hypervisor
- :acrn-commit:`86180bd` HV: Calling into VPCI init/unit functions for partition hypervisor
- :acrn-commit:`65bd038` HV: Compiling in VCPI code for partition hypervisor
- :acrn-commit:`f60fcb6` HV: Defining the per-vm static vpci table for partition hypervisor
- :acrn-commit:`2b22e88` hv: init: rm the code of creating guest init page table
- :acrn-commit:`33e1149` hv: init: unify init logic for vm0 bsp
- :acrn-commit:`4acce93` hv: move save_segment/load_segment to a header file
- :acrn-commit:`43db87c` hv: rename acrn_efi.h to vm0_boot.h
- :acrn-commit:`adddf51` hv: move define of struct cpu_gp_regs to a separate headfile
- :acrn-commit:`5a5b2a1` hv: init: save boot context from bootloader/bios
- :acrn-commit:`ac39b90` DM: update GSI sharing info
- :acrn-commit:`2fc3bde` HV: trusty: new hypercall to save/restore context of secure world
- :acrn-commit:`3225b16` HV: trusty: log printing cleanup
- :acrn-commit:`9ba14da` HV: trusty: remove unused HC ID
- :acrn-commit:`b5b769f` HV: trusty: refine secure_world_control
- :acrn-commit:`ff96453` hv: Boot multiple OS for Partitioning mode ACRN
- :acrn-commit:`5e32c02` tools:acrn-crashlog: Enhance some functions
- :acrn-commit:`10f0bb0` hv: remove push/pop instruction emulation.
- :acrn-commit:`fa9fec5` hv: inject invalid opcode if decode instruction fails
- :acrn-commit:`1a00d6c` hv: add more exception injection API
- :acrn-commit:`96e99e3` hv: use more reliable method to get guest DPL.
- :acrn-commit:`63fe48c` hv: get correct fault address for copy_to/from_gva
- :acrn-commit:`55105db` DM: notify VHM request complete after pausing the VM
- :acrn-commit:`4753da4` doc: add interrupt high-level design doc
- :acrn-commit:`11c209e` DM: add tag info while no repo in release
- :acrn-commit:`8af90e0` misc: Remove unnecessary ExecStop in systemd services
- :acrn-commit:`4106fad` hv: treewide: fix 'Switch empty default has no comment'
- :acrn-commit:`af7943c` DM: check more in guest service & launch script
- :acrn-commit:`04b4c91` hv: Adding a wrapper on top of prepare_vm0
- :acrn-commit:`638d714` DM: adapt to the new VHM request state transitions
- :acrn-commit:`ea13758` DM: add wrappers to gcc built-in atomic operations
- :acrn-commit:`c0544c9` hv: treewide: fix 'Potential side effect problem in expression'
- :acrn-commit:`b1612e3` add cpu_do_idle to handle idle
- :acrn-commit:`b78aa34` HV: instr_emul: Make vm_update_register/rflags as void
- :acrn-commit:`12726db` HV: instr_emul: Make vie_read/write_bytereg as non-failed function
- :acrn-commit:`59c0f35` HV: instr_emul: Make vm_set/get_register as non-failed function
- :acrn-commit:`b6b7e75` HV: instr_emul: Make vm_get_seg_desc a void function
- :acrn-commit:`e625bd7` HV: vmx code clean up
- :acrn-commit:`820b5e4` HV: instr_emul: Remove dead code
- :acrn-commit:`f03ae8d` HV: instr_emul: Rearrange logic of instr_emul*
- :acrn-commit:`ce79d3a` HV: instr_emul: Handle error gracefully
- :acrn-commit:`8836abe` HV: instr_emul: Unify params passing to emulate_xxx
- :acrn-commit:`cebc8d9` DM USB: xHCI: Refine drd code to fix a potential NULL pointer issue.
- :acrn-commit:`7109ab4` hv:removed assert in free_ept_mem
- :acrn-commit:`a5121e9` dm: uart: add state check of backend tty before uart_closetty
- :acrn-commit:`fe51acf` Revert "[REVERT-ME]:handle discontinuous hpa for trusty"
- :acrn-commit:`63ef123` move global x2apic_enabled into arch dir
- :acrn-commit:`72f9c9a` pm: use cpu_context for s3 save/restore
- :acrn-commit:`8a95b2a` vcpu: add ext context support for world switch
- :acrn-commit:`3d5d6c9` vcpu: add get/set register APIs
- :acrn-commit:`5aa1ad3` HV:treewide:fix value outside range of underlying type
- :acrn-commit:`c663267` hv: timer: request timer irq once only
- :acrn-commit:`b4a2ff5` hv: treewide: fix 'Prototype and definition name mismatch'
- :acrn-commit:`f42878e` hv: apicv: improve the default apicv reset flow
- :acrn-commit:`6e86d48` hv: vioapic: set remote IRR to zero once trigger mode switch to edge
- :acrn-commit:`1e18867` hv: vioapic: remove EOI register support
- :acrn-commit:`f96f048` hv: vioapic: change the variable type of pin to uint32_t
- :acrn-commit:`b13882f` hv: vioapic: improve the vioapic reset flow
- :acrn-commit:`86de47b` hv: vioapic: correct the ioapic id mask
- :acrn-commit:`68cbdb3` hv: vioapic: avoid deliver unnecessary interrupt for level trigger
- :acrn-commit:`771c6db` hv: vioapic: refine vioapic_mmio_rw function
- :acrn-commit:`f0d2291` hv: vioapic: check vector prior to irr in EOI write emulation
- :acrn-commit:`fc41629` hv: vioapic: refine vioapic mmio access related code
- :acrn-commit:`66814d8` tools: fix resuming vm issue in acrnctl
- :acrn-commit:`7b34ae8` tools: fix resuming vm issue in acrnd
- :acrn-commit:`6cd6e3d` tools: fix an issue acrnd does not notify the vm stop state to cbc lifecycle service
- :acrn-commit:`331300d` tools: fix an invalid parameter of send_msg in query_state
- :acrn-commit:`7345677` hv:cleanup vmid related code
- :acrn-commit:`2299926` HV: Refine 'hv_main()' function usage
- :acrn-commit:`9d9c97d` doc: fix table in acrn-shell documentation
- :acrn-commit:`093f2f9` Update acrn-shell.rst
- :acrn-commit:`9689227` Update acrn-shell.rst
- :acrn-commit:`f9bf917` HV: Refine hypervisor shell commands
- :acrn-commit:`6643adf` HV: Adding mptable support for partition mode ACRN
- :acrn-commit:`fd0c918` hv: treewide: fix 'Procedure parameter has a type but no identifier'
- :acrn-commit:`c27e250` HV: instr_emul: Move op_byte from vie_op to instr_emul_vie
- :acrn-commit:`baf055e` HV: instr_emul: Using size2mask array directly
- :acrn-commit:`b6a0a36` HV: instr_emul: Remove vie_read_register
- :acrn-commit:`3702659` HV: Rename functions, variables starting with "_"
- :acrn-commit:`a71dede` hv: treewide: fix 'Array has no bounds specified'
- :acrn-commit:`a3b44a2` hv:Replace 0(cpu_id) with BOOT_CPU_ID
- :acrn-commit:`7a3d03c` dm: uart: fix acrn-dm crash issue when invoke uart_closetty function
- :acrn-commit:`8f39a22` hv: cpu: remove unnecessary cpu_id valid check
- :acrn-commit:`a98113b` HV: fully check VMCS control settings
- :acrn-commit:`ae8836d` hv:fix return value violation for vioapic_get_rte
- :acrn-commit:`cd3a62f` HV: Refine invalid parameter handling in hypervisor shell
- :acrn-commit:`61782d7` hv:Rename port/mmio read and write APIs
- :acrn-commit:`7db4c0a` DM: Add function to update PM_WAK_STS
- :acrn-commit:`a8a27d8` dm: add S3 support for UOS
- :acrn-commit:`8ee4c0b` DM: add vm_stop/reset_watchdog
- :acrn-commit:`a2241d9` DM: register pm ops to monitor
- :acrn-commit:`f576f97` hv: add vm restart API
- :acrn-commit:`a4eebb0` hv: cleanup inline assembly code in vmx.c a little bit
- :acrn-commit:`77c3917` HV:treewide:avoid using multiple # or ## in a macro
- :acrn-commit:`581a336` HV: Add Partitioning mode option for ACRN
- :acrn-commit:`93ed037` hv:cleanup console/uart code
- :acrn-commit:`22005c6` HV: Refine hypervisor shell commands
- :acrn-commit:`1664e0c` HV:fix rest integer violations
- :acrn-commit:`56904bc` doc: CSS tweak for table caption location
- :acrn-commit:`64f6295` acrn-manager: create acrn-hypervisor-dev package
- :acrn-commit:`51c75e9` hv: treewide: fix 'Function prototype/defn param type mismatch'
- :acrn-commit:`cf8fd8c` Revert "HV: clear memory region used by UOS before it exit"
- :acrn-commit:`9c24c5c` HV:Rename 'shell_internal.c' to 'shell.c'
- :acrn-commit:`3b06282` HV:Remove i/o session sw interface from hypervisor shell
- :acrn-commit:`a8e9d83` samples: change WIFI BDF to 3:0:0
- :acrn-commit:`00bfde3` HV: rename resume_vm to start_vm in hypercall api
- :acrn-commit:`5e31e7c` IOC mediator: Add parking brake and Hvac signals
- :acrn-commit:`457ecd6` hv: softirq: refine softirq
- :acrn-commit:`073583c` hv: softirq: move softirq.c to common directory
- :acrn-commit:`dec24a9` hv: add check to invalid CR8 writing from guest
- :acrn-commit:`13a50c9` hv: Explicitly trap VMXE and PCIDE bit for CR4 write
- :acrn-commit:`f0ef41c` hv: Extend the always off mask of CR0 and CR4
- :acrn-commit:`d18642a` hv: Add function to check whether cr0 written operation is valid
- :acrn-commit:`ce7257e` doc: tweak logo href to projectacrn.org
- :acrn-commit:`6d25535` doc: fix doc errors from acrn_vhm_mm.h API changes
- :acrn-commit:`014bef6` doc: add virtio-console HLD document
- :acrn-commit:`50af102` dm: bios: update vSBL binary to v0.8
- :acrn-commit:`87a4abd` tools: acrn-crashlog: fix build warnings with gcc8.1.1
- :acrn-commit:`6e77a8d` HV:treewide:rename enum vpic_wire_mode, stack_canary, segment_override, pde_index
- :acrn-commit:`52fe9f4` hv: use macro instead of specify number
- :acrn-commit:`8ed98d3` DM: fix make install issue in auto boot UOS service
- :acrn-commit:`8e2c730` HV:VLAPIC:add suffix "_fn" for function pointer
- :acrn-commit:`2c95a8c` HV:treewide:rename struct pic and iommu_domain
- :acrn-commit:`17771c0` HV: io: refine state transitions of VHM requests
- :acrn-commit:`941eb9d` HV: io: move I/O emulation post-work to io.c
- :acrn-commit:`d817951` HV: io: add post-work for PCICFG and WP requests
- :acrn-commit:`26ab2c9` HV: io: move MMIO handler registration to io.c
- :acrn-commit:`b21b172` HV: io: refactoring vmexit handler on EPT violation
- :acrn-commit:`50e4bc1` HV: io: refactoring vmexit handler on I/O instruction
- :acrn-commit:`d4d8a12` doc: tweak formatting for :kbd: role
- :acrn-commit:`9c3d77e` doc: tweak known-issues pattern for hypercall API
- :acrn-commit:`99ebd92` hv:Delete serial files
- :acrn-commit:`ae30040` hv:Reshuffle console/uart code
- :acrn-commit:`b743627` IOC mediator: fix IOC mediator blocks acrn-dm shutdown flow
- :acrn-commit:`159d57b` HV:treewide:rename union lapic_id and struct segment
- :acrn-commit:`c477211` HV:treewide:rename struct key_info, pir_desc, map_params
- :acrn-commit:`f614fcf` hv: debug: add CR4 to vcpu_dumpreg output
- :acrn-commit:`8205c9a` HV:INSTR_EMUL:Rename struct vie, vie_op, and emul_ctxt
- :acrn-commit:`3446e84` HV:treewide:rename struct timer as struct hv_timer
- :acrn-commit:`cf7a940` HV: clear memory region used by UOS before it exit
- :acrn-commit:`a2fe964` HV: Rename functions beginning with "_"
- :acrn-commit:`d40a6b9` DM: add service to support auto boot UOS
- :acrn-commit:`496e400` HV:treewide:fix rest of violations related parameter changed
- :acrn-commit:`42c77e4` Documentation: add needed library for acrnprobe
- :acrn-commit:`a4aed45` tools: acrn-crashlog: replace debugfs with api
- :acrn-commit:`ea8cb41` tools: acrn-crashlog: replace fdisk and losetup with api
- :acrn-commit:`134e79a` tools: acrn-crashlog: New apis to replace debugfs
- :acrn-commit:`db05675` tools: acrn-crashlog: New apis to replace losetup and fdisk
- :acrn-commit:`c01e675` HV:VLAPIC:rename variable vlapic_timer in the struct and function
- :acrn-commit:`59771ff` HV:treewide:fix "Reference parameter to procedure is reassigned"
- :acrn-commit:`9d4c9d7` HV: stop retrieving seed from multiboot modules
- :acrn-commit:`1b527e5` HV: parse seed through cmdline during boot stage
- :acrn-commit:`58b42ba` HV:treewide:rename struct vpic as struct acrn_vpic
- :acrn-commit:`33fdfd0` HV:treewide:rename struct vlapic as struct acrn_vlapic
- :acrn-commit:`9ea50a5` acrn.conf: remove maxcpus from cmdline
- :acrn-commit:`10ed599` HV: cleanup sprintf&string.c MISRA-C issues
- :acrn-commit:`88f74b5` HV: io: unify vhm_request req and mem_io in vcpu
- :acrn-commit:`1915eec` HV: io: separate I/O emulation interface declarations
- :acrn-commit:`3cab926` DM: add param: -V 5 to auto check/boot UOS image
- :acrn-commit:`45d6f72` HV:refine 'create_vm()' to avoid potential crash and memory leak
- :acrn-commit:`53a5941` doc: add GVT-G porting guide
- :acrn-commit:`746cbab` doc: add UART virtualization documentation
- :acrn-commit:`6c54cba` doc: cleanup css, search, version choices
- :acrn-commit:`f815415` hv: ept: add lookup_address to lookup the page table
- :acrn-commit:`e2516fa` hv: mmu: reimplement mmu_add to add page table mapping
- :acrn-commit:`c779958` hv: mmu: replace the old mmu_del
- :acrn-commit:`236bb10` hv: mmu: refine delete page table mapping
- :acrn-commit:`34c6862` hv: hypercall: add support to change guest page write permission
- :acrn-commit:`efd5ac4` hv: mmu: fix wrong to modify a large page attributes
- :acrn-commit:`5189bcd` HV:treewide:fix "Attempt to change parameter passed by value"
- :acrn-commit:`e71a088` samples: offline SOS cpus before launch uos
- :acrn-commit:`d5ead61` samples: remove maxcpus from bootargs
- :acrn-commit:`2dca23c` add hypercall hc_sos_offline_cpu support
- :acrn-commit:`589c723` add CONFIG_VM0_DESC support
- :acrn-commit:`2283378` refine definition for foreach_vcpu
- :acrn-commit:`3117870` hv:Change shell_init to void type
- :acrn-commit:`a1923dd` hv: add a missing semicolon in vmexit.c
- :acrn-commit:`6788c09` hv: bug fix on operating spin_lock
- :acrn-commit:`ff05a6e` hv:Remove dead code in console.c
- :acrn-commit:`a661ffa` fix x86 dir integer violations
- :acrn-commit:`f1b9f5a` hv: cpu: using struct cpu_gp_regs for general-purpose regs in inter_excp_ctx
- :acrn-commit:`586b527` hv: cpu: remove general-purpose register mapping in instruction emulation
- :acrn-commit:`b2802f3` hv: cpu: align general-purpose register layout with vmx
- :acrn-commit:`3d6ff0e` tools: acrntrace: save trace data file under current dir by default
- :acrn-commit:`3abfdba` doc: add script for syncing acrn-kernel for API gen
- :acrn-commit:`363a84c` DOC:GSG: Fix few mistakes about updating acrn.conf and efibootmgr options
- :acrn-commit:`f18a02a` HV: MISRA cleanup for platform acpi info
- :acrn-commit:`ee13110` HV: change wake vector address to accommodate sbl
- :acrn-commit:`4344832` Revert "DM sample: force enabling HDMI1 and HDMI2 connectors"
- :acrn-commit:`f7f04ba` hv: mmu: minor fix about hv mmu && ept modify
- :acrn-commit:`502e3e2` hv: mmu: refine set guest memory region API
- :acrn-commit:`27fbf9b` HV:treewide:Fixing pointer castings
- :acrn-commit:`a368b57` hv: fix typo in relocation code
- :acrn-commit:`b35e330` HV: make: check CONFIG_RELEASE=y for release build
- :acrn-commit:`da0f28c` HV: Bracket for the same level of precedence
- :acrn-commit:`91337da` HV: logical and high level precedence expression needs brackets
- :acrn-commit:`7aec679` HV: Clean up the unused or legacy code-like comment
- :acrn-commit:`c776137` doc: fix doc error filter patterns
- :acrn-commit:`fb8bce1` hv: treewide: fix 'Array has no bounds specified'
- :acrn-commit:`af194bc` HV: fix bug of restore rsp context
- :acrn-commit:`4fd870f` hv: efi: remove multiple defined struct efi_ctx & dt_addr_t
- :acrn-commit:`d5be735` hv: correct the way to check if a MSR is a fixed MTRR register
- :acrn-commit:`bd69799` fix assign.c integer violations
- :acrn-commit:`f0a3585` HV: common: cleanup of remaining integral-type issues
- :acrn-commit:`112b5b8` HV: guest: cleanup of remaining integral type violations
- :acrn-commit:`1a1ee93` HV: hypercall: make hypercall functions return int32_t
- :acrn-commit:`ad73bb5` HV: treewide: unify the type of bit-field members
- :acrn-commit:`c0b55cd` HV:vtd:fix all integer related violations
- :acrn-commit:`4c941ed` HV:vtd.h fixed inline function violations
- :acrn-commit:`a17653b` HV:transfer DMAR_[GS]ET_BITSLICE to inline function
- :acrn-commit:`e2ad788` doc: clean up tools docs
- :acrn-commit:`38b9b7d` HV: cpuid: Disable Intel RDT for guest OS
- :acrn-commit:`9ac1be2` DM USB: enable isochronous transfer
- :acrn-commit:`b95f939` DM USB: temporary solution for corner case of control transfer
- :acrn-commit:`3389e83` DM USB: add some BCD codes
- :acrn-commit:`b9597d4` DM USB: xHCI: add microframe index(MFINDEX) register emulation support
- :acrn-commit:`a49d483` DM USB: process LIBUSB_TRANSFER_STALL error
- :acrn-commit:`640d896` DM USB: change TRB ring processing logic for ISOC transfer
- :acrn-commit:`d24213d` DM USB: xHCI: fix xhci speed emulation logic
- :acrn-commit:`d6cc701` DM USB: refine logic of toggling interface state
- :acrn-commit:`5317124` DM USB: xHCI: add support for USB 3.0 devices
- :acrn-commit:`8317dea` DM USB: fix guest kernel short packets warning
- :acrn-commit:`7431a90` DM USB: add code for error processing
- :acrn-commit:`00fbfd6` DM USB: fix an USB endpoint reset flow issue
- :acrn-commit:`cb93887` DM USB: modify some logs to help debug
- :acrn-commit:`aecb67b` DM USB: support multiple interfaces USB device
- :acrn-commit:`38e2e45` hv: ept: move EPT PML4 table allocation to create_vm
- :acrn-commit:`1815a1b` hv: ept: store virtual address of EPT PML4 table
- :acrn-commit:`23a5c74` HV: handle integral issues as MISRA-C report
- :acrn-commit:`0252ae9` hv: treewide: fix 'No definition in system for prototyped procedure'
- :acrn-commit:`d28fff2` HV:treewide:Update the type of return value and parameters of atomic operations
- :acrn-commit:`3aa7d59` hv: check eptp value before calling free_ept_mem()
- :acrn-commit:`3571afc` HV: hypercall: revisit types in structure parameters
- :acrn-commit:`f691cab` HV: treewide: terminate 'if .. else if' constructs with 'else'
- :acrn-commit:`e13c852` HV:INSTR_EMUL: Clean up CPU_reg_name
- :acrn-commit:`f4ca3cc` hv: instr_emul: fix 'Parameter indexing array too big at call'
- :acrn-commit:`84d320d` HV:treewide:Fix type conversion in VMX, timer and MTTR module
- :acrn-commit:`f7efd0f` hv: mmu: replace modify_mem with mmu_modify
- :acrn-commit:`0a33c0d` hv: mmu: replace ept_update_mt with ept_mr_modify
- :acrn-commit:`1991823` hv: mmu: revisit mmu modify page table attributes
- :acrn-commit:`20c80ea` HV: bug fix on emulating msi message from guest
- :acrn-commit:`9695d3b` tools: replace payload[0] of struct mngr_msg with an union
- :acrn-commit:`ec86009` tools: acrn-manager: code cleanup
- :acrn-commit:`be80086` tools: Makefile: fix lack of dependence for acrm_mngr.h
- :acrn-commit:`a257f2f` HV: Fixes index out of bounds for addressing irq.
- :acrn-commit:`988a3fe` doc: use code-block:: none for command examples
- :acrn-commit:`dc6d775` tools: acrnd: update README.rst
- :acrn-commit:`0631473` [doc] Add API document for ACRN-GT
- :acrn-commit:`7e9b7f6` HV: instr_emul: Replace ASSERT/panic with pr_err
- :acrn-commit:`f912953` HV:treewide:Update exec_vmread/exec_vmwrite and exec_vmread64/exec_vmwrite64
- :acrn-commit:`612cdce` HV:treewide:Add exec_vmread32 and exec_vmwrite32 functions
- :acrn-commit:`6543796` HV:treewide: Add exec_vmread16 and exec_vmwrite16 functions
- :acrn-commit:`d3b9712` HV:INSTR:Rearrange register names in the enum cpu_reg_name
- :acrn-commit:`055153b` HV:treewide:Replace HOST_GDT_RING0_CODE/DATA_SEL with constant
- :acrn-commit:`f2774e4` HV:common:fix "integer type violations"
- :acrn-commit:`aa2b2d8` hv: change several APIs to void type
- :acrn-commit:`8017ebd` HV:vtd:dma change the macro to the inline function
- :acrn-commit:`d8c3765` HV:vtd:cap change the macro to the inline function
- :acrn-commit:`69ebf4c` HV: vioapic: cleaning up integral-type-related violations
- :acrn-commit:`a1069a5` HV: ioapic: unify the access pattern to RTEs
- :acrn-commit:`9878543` DM: add system reset (with RAM content kept)
- :acrn-commit:`b33012a` DM: add vm reset API
- :acrn-commit:`8d12c06` dm: introduce system/full reset and suspend
- :acrn-commit:`76662a6` loader: Update the memory address of GUEST_CFG_OFFSET
- :acrn-commit:`a91952d` HV: per_cpu: drop dependency on version.h and add license header
- :acrn-commit:`116038f` HV: make: consider header dependencies when rebuilding
- :acrn-commit:`11239ae` update launch_uos.sh to align with ACRN v0.1
- :acrn-commit:`b2e676a` update kernel-pk version to align with ACNR v0.1
- :acrn-commit:`ea0bbd5` doc: reorganize doc tree
- :acrn-commit:`e042558` doc: update GSG for v0.1, add console code-block
- :acrn-commit:`1c712c5` delete pci_devices_ignore=(0:18:1)
- :acrn-commit:`2f2d108` HV: handle integral issue report by MISRA-C
- :acrn-commit:`7706e5c` tools: acrnd: store/load timer list
- :acrn-commit:`e435f03` tools: acrnd: handle timer request from UOS
- :acrn-commit:`ee9ec9d` tools: acrnd: the acrnd work list
- :acrn-commit:`f5e9c76` tools: acrnd: handle resume request from SOS-LCS
- :acrn-commit:`04ed916` tools: acrnd: handle stop request from SOS-LCS
- :acrn-commit:`bcb101f` tools: acrnd: the daemon for acrn-manager
- :acrn-commit:`c4f9a2f` tools: rework on vm ops
- :acrn-commit:`f0fe17d` hv: sprintf: fix 'Declaration does not specify an array'
- :acrn-commit:`aa5027a` HV:misc:fix "signed/unsigned conversion with cast"
- :acrn-commit:`619c600` hv: cpu state update should be moved just before halt.
- :acrn-commit:`621425d` hv: further fix to configurable relocation
- :acrn-commit:`944776f` HV: Fix new MISRAC violations for brackets
- :acrn-commit:`90b342b` HV: prototyping non-static function
- :acrn-commit:`8925eb5` hv: set guest segment base to zero if VCPU does not start in real mode
- :acrn-commit:`b831120` HV: coding style cleanup for TRACE_2L & TRACE_4I usage
- :acrn-commit:`c808972` hv: fix the potential dead loop in _parse_madt
- :acrn-commit:`4627cd4` HV: build: drop useless files
- :acrn-commit:`680c64d` HV:transfer vmid's type to uint16_t
- :acrn-commit:`6ad1508` dm: virtio-net: add variable name in function declaration
- :acrn-commit:`cb0009f` hv: cpu: fix 'Pointer arithmetic is not on array'
- :acrn-commit:`44a175e` HV: instr_emul: Add new function vie_update_rflags
- :acrn-commit:`2f3eb67` HV: Remove SIB decode related code in decode_modrm
- :acrn-commit:`0fbdf37` HV: instr_emul: Cleanup ASSERT
- :acrn-commit:`e3302e8` HV:transfer vm_hw_logical_core_ids's type and rename it
- :acrn-commit:`1d628c6` hv:fix MISRA-C return value violation
- :acrn-commit:`2a2adc7` HV:CPU:Fix a mistake introduced by MARCO replacing patch
- :acrn-commit:`e3452cf` HV: vlapic: minimize explicit casts by adjusting types
- :acrn-commit:`e08a58e` HV: vlapic: save complex expressions to local variables
- :acrn-commit:`f05e2fc` HV: vlapic: cleanup types in formatting strings
- :acrn-commit:`6dd78d5` HV: vlapic: convert loop variables to unsigned
- :acrn-commit:`87f2d4c` HV: vlapic: add suffix 'U' when necessary
- :acrn-commit:`1af8586` HV: Fix missing brackets for MISRA C Violations
- :acrn-commit:`af806a9` HV: Fix missing brackets for MISRA C Violations
- :acrn-commit:`4aa6cda` HV: Fix missing brackets for MISRA C Violations
- :acrn-commit:`d16d9e5` HV: Fix missing brackets for MISRA C Violations
- :acrn-commit:`82e0cdb` HV: Fix missing brackets for MISRA C Violations
- :acrn-commit:`dbfd0e5` HV: Fix missing brackets for MISRA C Violations
- :acrn-commit:`88a3205` HV: Fix missing brackets for MISRA C Violations
- :acrn-commit:`b4a6b93` doc: add v0.1 doc choice
- :acrn-commit:`3fe0fed` version: 0.2-unstable
- :acrn-commit:`b4fb261` hv: fix bug in some embedded assembly code in vmx
- :acrn-commit:`8336101` DM: Fix potential buffer overflow and uninitialized variable
- :acrn-commit:`194fd8b` hv: irq: fix 'Pointer arithmetic is not on array'
- :acrn-commit:`401ffd1` HV: pm: cleanup for MISRA integral type violations
- :acrn-commit:`202bc54` HV: trusty: revise trusty_boot_param structure
- :acrn-commit:`b30ba3d` tools:acrn-crashlog: Detect and classify the crash in ACRN and kernel
- :acrn-commit:`a5853d6` tools:acrn-crashlog: Improve the process of crash reclassify
- :acrn-commit:`0683b16` tools:acrn-crashlog: Get reboot reason in acrnprobe
- :acrn-commit:`2d03706` hv:change shell_puts to void type
- :acrn-commit:`4cab8b9` HV: code cleanup as MISRA-C report for guest/vmsr
- :acrn-commit:`8c43ad5` HV: add the missing brackets to loop body
- :acrn-commit:`fd81655` HV: add the missing brackets to loop body
- :acrn-commit:`df038fc` HV: vmx: Change variable field to uint32_t
- :acrn-commit:`43e4bd4` version: v0.1

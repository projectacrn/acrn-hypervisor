.. _release_notes_0.8:

ACRN v0.8 (Apr 2019)
####################

We are pleased to announce the release of Project ACRN version 0.8.

ACRN is a flexible, lightweight reference hypervisor, built with
real-time and safety-criticality in mind, optimized to streamline
embedded development through an open source platform. Check out the
:ref:`introduction` for more information.


All project ACRN source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, tools, and
documentation.  You can either download this source code as a zip or
tar.gz file (see the `ACRN v0.8 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v0.8>`_ or
use Git clone and checkout commands:

.. code-block:: bash

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v0.8

The project's online technical documentation is also tagged to correspond
with a specific release: generated v0.8 documents can be found at
https://projectacrn.github.io/0.8/.  Documentation for the latest
(master) branch is found at https://projectacrn.github.io/latest/.

ACRN v0.8 requires Clear Linux OS version 28600. Please follow the
instructions in the :ref:`getting-started-apl-nuc`.

Version 0.8 new features
************************

GPIO virtualization
=========================

GPIO virtualization is supported as para-virtualization based on the
Virtual I/O Device (VIRTIO) specification. The GPIO consumers of the
Front-end are able to set or get GPIO values, directions, and
configuration via one virtual GPIO controller. In the Back-end, the GPIO
command line in the launch script can be modified to map native GPIO to
UOS.

Enable QoS based on runC container
==================================
ACRN supports Device-Model QoS based on runC container to control the SOS
resources (CPU, Storage, MEM, NET) by modifying the runC configuration file.

S5 support for RTVM
===============================
ACRN supports a Real-time VM (RTVM) shutting itself down. A RTVM is a
kind of VM that the SOS can't interfere at runtime, and as such, can
only power itself off internally. All poweroff requests external to the
RTVM will be rejected to avoid any interference.

Document updates
================
Several new documents have been added in this release, including:

* :ref:`Zephyr RTOS as Guest OS <using_zephyr_as_uos>`
* :ref:`Enable cache QoS with CAT <using_cat_up2>`
* :ref:`ACRN kernel parameter introduction <kernel-parameters>`
* :ref:`faq` update for two issues
* :ref:`ACRN Debug introduction <acrn-debug>`

New Features Details
********************

- :acrn-issue:`923` - GPU Mediator shall be compatible with operation of graphics safety watchdog
- :acrn-issue:`1409` - Add support for profiling [sep/socwatch tools]
- :acrn-issue:`1568` - Implement PCI emulation functionality in HV for UOS passthrough devices and SOS MSI/MSI-X remapping
- :acrn-issue:`1867` - vMSR code reshuffle
- :acrn-issue:`2020` - DM: Enable QoS in ACRN, based on runC container
- :acrn-issue:`2512` - GPIO virtualization
- :acrn-issue:`2611` - hv: search additional argument when parsing seed from ABL.
- :acrn-issue:`2868` - OVMF release v0.8
- :acrn-issue:`2708` - one binary for SBL and UEFI
- :acrn-issue:`2713` - Enable ACRN to boot Zephyr
- :acrn-issue:`2792` - Pass ACRN E820 map to OVMF
- :acrn-issue:`2865` - support S5 of Normal VM with lapic_pt

Fixed Issues Details
********************

- :acrn-issue:`1996` - [APLNUC/KBLNUC/APLUP2]There is an error log when using "acrnd&" to boot UOS
- :acrn-issue:`2052` - tpm_emulator code reshuffle
- :acrn-issue:`2474` - Need to capture dropped sample info while profiling
- :acrn-issue:`2490` - systemd virtualization detection doesn't work
- :acrn-issue:`2522` - Start ias in SOS, no display
- :acrn-issue:`2523` - UOS monitor does not display when using ias
- :acrn-issue:`2524` - [UP2][SBL] Launching UOS hang while weston is running in SOS
- :acrn-issue:`2597` - Return PIPEDSL from HW register instead of cached memory for Guest VGPU
- :acrn-issue:`2704` - Possible memory leak issues
- :acrn-issue:`2760` - [UP2]{SBL] make APL-UP2 SBL acrn-hypervisor sos image failed.
- :acrn-issue:`2772` - Enable PCI-E realtek MMC card for UOS
- :acrn-issue:`2780` - [APL_NUC KBL_NUC EFI_UP2]Update clear Linux missing acrn.efi file
- :acrn-issue:`2792` - Pass ACRN E820 map to OVMF
- :acrn-issue:`2829` - The ACRN hypervisor shell interactive help is rather terse
- :acrn-issue:`2830` - Warning when building the hypervisor
- :acrn-issue:`2851` - [APL/KBL/UP2][HV][LaaG]Uos cannot boot when acrnctl add Long_VMName of more than 26
- :acrn-issue:`2870` - Use 'sha512sum' for validating all virtual bootloaders

Known Issues
************

:acrn-issue:`1773` - USB Mediator: Can't find all devices when multiple USB devices connected [Reproduce rate:60%]
   After booting UOS with multiple USB devices plugged in, there's a 60% chance that one or more devices are not discovered.

   **Impact:** Cannot use multiple USB devices at same time.

   **Workaround:** Unplug and plug-in the unrecognized device after booting.

-----

:acrn-issue:`1991` - Input not accepted in UART Console for corner case
   Input is useless in UART Console for a corner case, demonstrated with these steps:

   1) Boot to SOS
   2) ssh into the SOS.
   3) use ``./launch_UOS.sh`` to boot UOS.
   4) On the host, use ``minicom -s dev/ttyUSB0``.
   5) Use ``sos_console 0`` to launch SOS.

   **Impact:** Fails to use UART for input.

   **Workaround:** Enter other keys before typing :kbd:`Enter`.

-----

:acrn-issue:`2267` - [APLUP2][LaaG] LaaG can't detect 4k monitor
   After launching UOS on APL UP2 , 4k monitor cannot be detected.

   **Impact:** UOS can't display on a 4k monitor.

   **Workaround:** Use a monitor with less than 4k resolution.

-----

:acrn-issue:`2278` - [KBLNUC] Cx/Px is not supported on KBLNUC
   C states and P states are not supported on KBL NUC.

   **Impact:** Power Management state-related operations in SOS/UOS on
   KBL NUC can't be used.

   **Workaround:** None

-----

:acrn-issue:`2279` - [APLNUC] After exiting UOS, SOS can't use USB keyboard and mouse
   After exiting UOS with mediator
   Usb_KeyBoard and Mouse, SOS cannot use the USB keyboard and mouse.

   These steps reproduce the issue:

   1) Insert USB keyboard and mouse in standard A port (USB3.0 port)
   2) Boot UOS by sharing the USB keyboard and mouse in cmd line:

      ``-s n,xhci,1-1:1-2:1-3:1-4:2-1:2-2:2-3:2-4 \``

   3) UOS access USB keyboard and mouse.
   4) Exit UOS.
   5) SOS tries to access USB keyboard and mouse, and fails.

   **Impact:** SOS cannot use USB keyboard and mouse in such case.

   **Workaround:** Unplug and plug-in the USB keyboard and mouse after exiting UOS.

-----

:acrn-issue:`2527` - System will crash after a few minutes running stress test ``crashme`` tool in SOS/UOS.
   System stress test may cause a system crash.

   **Impact:** System may crash in some stress situations.

   **Workaround:** None

-----

:acrn-issue:`2526` - Hypervisor crash when booting UOS with acrnlog running with mem loglevel=6
   If we use ``loglevel 3 6`` to change the mem loglevel to 6, we may hit a page fault in HV.

   **Impact:** Hypervisor may crash in some situation.

   **Workaround:** None

-----

:acrn-issue:`2753` - UOS cannot resume after suspend by pressing power key
   UOS cannot resume after suspend by pressing power key

   **Impact:** UOS may failed to resume after suspend by pressing the power key.

   **Workaround:** None


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

These commits have been added to the acrn-hypervisor repo since the v0.6
release in Feb 2019 (click on the CommitID link to see details):

.. comment

   This list is obtained from this git command (update the date to pick up
   changes since the last release):

   git log --pretty=format:'- :acrn-commit:`%h` - %s' --after="2018-03-01"

- :acrn-commit:`296c974d` - OVMF release v0.8
- :acrn-commit:`e7f77244` - Tools: acrnctl fix return value when it fails to execute the commands
- :acrn-commit:`eea0ecd2` - Tools: acrnctl fix return value when it fails to execute  the commands.
- :acrn-commit:`cee45a80` - hv: add default handlers for PIO/MMIO access
- :acrn-commit:`01b28c8e` - doc: Add tutorial about how to use CAT on UP2
- :acrn-commit:`79582b99` - doc: update software design guidelines
- :acrn-commit:`efad4963` - DM: Add -A to support S5 of hard rt vm
- :acrn-commit:`382acfaf` - HV: Using INIT to kick vCPUs off when RTVM poweroff by itself
- :acrn-commit:`2771b46b` - HV: Add one delmode parameter to make_reschedule_request
- :acrn-commit:`ef9be020` - HV: Introduce one new API send_single_init
- :acrn-commit:`8ad5adce` - HV: Set vm state as with VM_POWERING_OFF when RTVM poweroff by itself
- :acrn-commit:`83d11bbf` - HV: Register S5 pio handler for dm-launched RTVM
- :acrn-commit:`1c0d7f78` - HV: HV: make io_read_fn_t return true or false
- :acrn-commit:`3b2ad677` - HV: make io_write_fn_t return true or false
- :acrn-commit:`ed286e32` - HV: Introduce a new API is_rt_vm
- :acrn-commit:`2e4d7eb5` - DM: Add new flag GUEST_FLAG_RT for RTVM
- :acrn-commit:`71f75ebf` - Tools: acrnlog: fix confusing message "bad file descriptor" when start acrnlog
- :acrn-commit:`9f234222` - ACRN: dm: Enable mount namespace for container.
- :acrn-commit:`dde326ec` - Acrn: dm: Add new start parameter in sample args
- :acrn-commit:`e91d7402` - hv: Debug messages from a CPU are overlapped with other CPUs messages
- :acrn-commit:`558a1788` - doc: update coding guidelines
- :acrn-commit:`868778a6` - hv: fix vulnerability when VM is destroyed
- :acrn-commit:`5a7be9b8` - tools: acrnctl fix cmd buffer is truncated when vmname too long
- :acrn-commit:`b2f2d952` - tools: acrnctl restrict length of vmname to 32 bytes
- :acrn-commit:`8109c2e9` - DM: restrict vmname size to 32 bytes
- :acrn-commit:`b1586ccc` - HV: move MAX_CONFIG_NAME_SIZE to acrn_common.h
- :acrn-commit:`c55308bd` - DM: use soft link of acrn_common.h in HV
- :acrn-commit:`06761102` - dm: remove smbios
- :acrn-commit:`3effbb05` - Revert "hv: vmsr: add IA32_MISC_ENABLE to msr store area"
- :acrn-commit:`40168e73` - hv: vlapic: remove TPR set/get API
- :acrn-commit:`4a683ed1` - hv: vlapic: minor fix for update_msr_bitmap_x2apic_apicv
- :acrn-commit:`20164799` - dm: leave a gap for 32-bit PCI hole in E820 map
- :acrn-commit:`3be6c659` - HV: merge partition_mode.c and sharing_mode.c's code into vpci.c
- :acrn-commit:`320bf183` - HV: rename pci_priv.h to vpci_priv.h
- :acrn-commit:`1a3c9b32` - HV: rename vpci files
- :acrn-commit:`c6a60dd2` - HV: remove all CONFIG_PARTITION_MODE from dm/vpci code
- :acrn-commit:`691468a3` - HV: Remove hypervisor.h from bsp folder
- :acrn-commit:`3d85d72f` - doc: update OVMF image location
- :acrn-commit:`d4ce780e` - doc: update the instructions to increase the size of a UOS disk image
- :acrn-commit:`80dc2c85` - doc: add some rules related to language extensions
- :acrn-commit:`3026a372` - DOC:Update standard reference of SW design guidelines
- :acrn-commit:`fddc5b91` - doc: update UP2 sample directory name in create-up2-image.sh
- :acrn-commit:`904c9e29` - doc: add more details to the FAQ (for version 0.7)
- :acrn-commit:`c47efa3f` - Add new FAQ
- :acrn-commit:`98b3d98a` - hv: vmsr: add IA32_MISC_ENABLE to msr store area
- :acrn-commit:`273381b3` - hv: vmsr: rename msr_num to msr_index in struct msr_store_entry
- :acrn-commit:`5585084c` - hv:move 'udelay' to timer.c
- :acrn-commit:`370998ba` - hv: replace MEM_2K with a new macro MAX_BOOTARGS_SIZE for bootargs size
- :acrn-commit:`12d97728` - DM: virtio-gpio: export GPIO ACPI device
- :acrn-commit:`014e611b` - DM: virtio-gpio: add IRQ statistics
- :acrn-commit:`83a98acb` - DM: virtio-gpio: support reading value from IRQ descriptor
- :acrn-commit:`d34b3ebd` - DM: virtio-gpio: emulate GPIO IRQ controller
- :acrn-commit:`92a0a399` - DM: virtio-gpio: GPIO IRQ initialization.
- :acrn-commit:`9480af8d` - DM: virtio-gpio: setup two virtqueues for gpio irq
- :acrn-commit:`e381aef2` - hv: seed: remove unused seed parsing source files
- :acrn-commit:`0947fbab` - HV: Fix a compiler warning in firmware.h
- :acrn-commit:`071ce15e` - dm: build E820 map for OVMF
- :acrn-commit:`4dd13310` - dm: remove empty UOS E820 entries
- :acrn-commit:`643513f3` - dm: update UOS default E820 map
- :acrn-commit:`263b486a` - dm: pci: add MMIO fallback handler for 64-bit PCI hole
- :acrn-commit:`82e42cfa` - dm: clean up mem.c
- :acrn-commit:`890d4022` - dm: remove GUEST_CFG_OFFSET
- :acrn-commit:`f97ba340` - Doc: Add tutorial about using zephyr as uos
- :acrn-commit:`410c76ac` - hv: enhance ACRN shell interactive help
- :acrn-commit:`a0de49d0` - hv: fix potential buffer overflow in sbl_init_vm_boot_info()
- :acrn-commit:`93ed2af1` - hv: passthru TSC_ADJUST to VM with lapic pt
- :acrn-commit:`f32b59d7` - hv: disable mpx capability for guest
- :acrn-commit:`71ce4c25` - HV: unify the sharing mode and partition mode coding style for similar functions
- :acrn-commit:`026250fd` - HV: centralize the pci cfg read/write sanity checking code
- :acrn-commit:`a403128a` - HV: remove vpci ops
- :acrn-commit:`aa1ee942` - HV: declare and export vpci ops functions as global instead of static local
- :acrn-commit:`a7f528cf` - HV: remove vdev ops for partition mode
- :acrn-commit:`b1cc1881` - hv: Use domain/device specific invalidation for DMAR translation caches
- :acrn-commit:`5c046879` - hv: minor fixes to a few calls to strncpy_s()
- :acrn-commit:`5fdc7969` - doc: add tutorial on how to increase the UOS disk size
- :acrn-commit:`657ac497` - doc: add rdmsr/wrmsr to the "ACRN Shell Commands" documentation
- :acrn-commit:`90b49375` - doc: add rules related to implementation-specific behaviors
- :acrn-commit:`e131d705` - hv: vmconfig: minor fix about regression of commit 79cfb1
- :acrn-commit:`9abd469d` - config: unify board names to lowercase
- :acrn-commit:`5398c901` - hv: remove CONFIG_PARTITION_MODE for pre-launched VM vE820 creation
- :acrn-commit:`ca6e3411` - HV: add vrtc for sharing mode
- :acrn-commit:`1b79f28e` - hv: update CR0/CR4 on demand in run_vcpu()
- :acrn-commit:`19c53425` - HV: remove vdev ops for sharing mode
- :acrn-commit:`eb4f4698` - HV: add const qualifier for the deinit vdev op functions
- :acrn-commit:`b2b1a278` - HV: remove intercepted_gpa and intercepted_size from struct pci_msix
- :acrn-commit:`5767d1e1` - HV: extract common code blocks to has_msi_cap and has_msix_cap functions
- :acrn-commit:`79cfb1cf` - hv: vmconfig: format guest flag with prefix GUEST_FLAG\_
- :acrn-commit:`c018b853` - hv: vmtrr: hide mtrr if hide_mtrr is true
- :acrn-commit:`906c79eb` - hv: vpci: restore vbdf when pci dev un-assigned from uos
- :acrn-commit:`7669a76f` - dm: passthru: pass pbdf when reset msi/msix interrupt
- :acrn-commit:`cd360de4` - hv: fix wrong comment message about CLOS usage in vm config
- :acrn-commit:`190b0940` - Makefile: build for apl-nuc by default
- :acrn-commit:`21d3dc68` - hv: seed: refine header file
- :acrn-commit:`0fb21cfa` - Tools: Acrnd fix reporting unnecessary error on NUC and UP2
- :acrn-commit:`ff41c008` - hv: trusty: refine control registers switching method
- :acrn-commit:`4157b843` - doc: add some rules related to naming convention
- :acrn-commit:`518a82d8` - hv: cleanup some hva/hpa conversion code
- :acrn-commit:`e9335fce` - doc: fix utf-8 punctuation, branding, spelling
- :acrn-commit:`9e78ad52` - doc: fix wrong description of trusty's memory mapping
- :acrn-commit:`fb3e47fd` - doc: add v0.7 version to master branch (/latest)
- :acrn-commit:`5e37c463` - version: 0.8-unstable
- :acrn-commit:`b147c5c6` - DM: Mark thre_int_pending as true when THR is empty
- :acrn-commit:`9b1e2f4c` - remove apl_sdc_stable branch story
- :acrn-commit:`53972001` - DM: fix memory leak
- :acrn-commit:`436c30e4` - doc: add 0.7 release notes
- :acrn-commit:`74023a9a` - hv: vtd: check bus number when assign/unassign device
- :acrn-commit:`93386d3c` - ACRN/DM: Destroy the created pci_device iterator to fix memory leak in passthru_init
- :acrn-commit:`31cb4721` - acrn/dm: Remove the memory leak in gvt mediator
- :acrn-commit:`065e16d3` - Makefile: make UP2 sample directory name consistent with board name
- :acrn-commit:`20249380` - audio-mediator: load updated audio kernel modules Audio kernel has updated name and add two new kernel modules from SOS 28100
- :acrn-commit:`95d1e402` - hv: refactor seed management
- :acrn-commit:`4d0419ed` - dm: passthru: fix potential mem leaks
- :acrn-commit:`caa291c0` - HV: some minor code cleanup for partition mode code
- :acrn-commit:`82789f44` - HV: declare and export partition mode's vdev functions as global instead of static local
- :acrn-commit:`93f6142d` - HV: declare and export sharing mode's vdev functions as global instead of static local
- :acrn-commit:`562628b9` - HV: remove the populate_msi_struct() function
- :acrn-commit:`3158c851` - HV: Modularize boot folder
- :acrn-commit:`286731d9` - hv:move instr_emul_ctxt instance to struct vcpu
- :acrn-commit:`5331b395` - hv:remove 'cpu_mode' from struct vm_guest_paging
- :acrn-commit:`ce387084` - hv: remove CONFIG_PLATFORM_[SBL|UEFI] and UEFI_STUB
- :acrn-commit:`334382f9` - efi-stub: minor change for uefi refactor
- :acrn-commit:`9b24620e` - hv: merge SBL and UEFI related stuff under boot
- :acrn-commit:`56d8b08b` - hv: merge SBL and UEFI related stuff under bsp
- :acrn-commit:`23e85ff1` - Makefile: remove deprecated PLATFORM
- :acrn-commit:`bd24e2de` - tools: acrnctl fix potential buffer overflow
- :acrn-commit:`78890622` - hv: vlapic: minor fix about detect_apicv_cap
- :acrn-commit:`f769f745` - hv: vlapic: add combined constraint for APICv
- :acrn-commit:`6f482b88` - dm: virtio: add memory barrier before notify FE
- :acrn-commit:`7ab6e7ea` - dm: usb: fix possible memory leak
- :acrn-commit:`694fca9c` - DM: Add sample script to launch zephyr as guest
- :acrn-commit:`204f9750` - tools: acrnd: Fix launch UOS by timer list without fork()
- :acrn-commit:`5d6f6ab7` - tools: acrn-manager: fix a race condition on updating VM state
- :acrn-commit:`d5ec844f` - tools: acrn-manager: Replace pdebug with explicit err msg
- :acrn-commit:`48774f71` - tools: acrn-manager: print more debug information
- :acrn-commit:`2b74e1a9` - HV: PAE: Add stac()/clac() in local_gva2gpa_pae
- :acrn-commit:`18ba7524` - dm: virtio-net: fix memory leak
- :acrn-commit:`31f04e1a` - doc: fix typos in coding guidelines
- :acrn-commit:`b75d5567` - Documentation: update the "ACRN Shell Commands" user guide
- :acrn-commit:`5f51e4a7` - pci.c: assert MSIX table count <= config max
- :acrn-commit:`137892fd` - hv: Remove multiple definitions for dmar translation structures
- :acrn-commit:`18b619da` - doc: add the doc for 'Error Detection and Handling'
- :acrn-commit:`72fbc7e7` - doc: add some comments for coding guidelines
- :acrn-commit:`e779982c` - doc: use the new board name for UP2 in create-up2-images.sh script
- :acrn-commit:`bf1aa5c1` - hv: destroy IOMMU domain after vpci_cleanup()
- :acrn-commit:`c0400b99` - HV: Fix modularization vm config code lost CAT code
- :acrn-commit:`649406b0` - HV: refine location of platform_acpi_info header
- :acrn-commit:`ff9ef2a1` - doc: fix broken external links
- :acrn-commit:`85b3ed3e` - doc: update the format in coding style part
- :acrn-commit:`f6a989b7` - dm: use power button acpi device to find its input event
- :acrn-commit:`55f52978` - hv:move several tsc APIs to timer.c
- :acrn-commit:`36f6a412` - hv:validate ID and state of vCPU for related APIs
- :acrn-commit:`9922c3a7` - HV: correct COM_IRQ default config type
- :acrn-commit:`741501c2` - hv: refine vlapic_calc_dest()
- :acrn-commit:`f572d1ec` - [RevertMe] dm: pci: restore workaround when alloc pci mem64 bar
- :acrn-commit:`ca3d4fca` - hv: vlapic: move LVT IRQ vector check to vlapic_fire_lvt
- :acrn-commit:`473d31c0` - hv: vlapic: add vector check for x2apic SELF IPI
- :acrn-commit:`e5d3a498` - hv: vlapic: call vlapic_accept_intr directly in vlapic_set_error
- :acrn-commit:`2b35c078` - hv: do EPT mapping only for physical memory backed GPA on pre-launched VMs
- :acrn-commit:`da14c961` - hv: simplify `get_primary_vcpu` and `vcpu_from_vid`
- :acrn-commit:`3d0d8609` - hv: vlapic: correct wrong use of vector
- :acrn-commit:`0943a836` - [hv] set ECX.bit31 to indicate the presence of a hypervisor
- :acrn-commit:`f6758fd6` - hv: fix a redundant check in general_sw_loader
- :acrn-commit:`b49df10a` - hv: Remove redundant get_dmar_info API calls
- :acrn-commit:`308d4e8c` - hv:move forward the initialization for  iommu & ptdev
- :acrn-commit:`bc107105` - doc: fix the typo related to tab


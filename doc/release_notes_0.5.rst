.. _release_notes_0.5:

ACRN v0.5 (Jan 2019)
####################

We are pleased to announce the release of Project ACRN version 0.5.

ACRN is a flexible, lightweight reference hypervisor, built with
real-time and safety-criticality in mind, optimized to streamline
embedded development through an open source platform. Check out the
:ref:`introduction` for more information.


All project ACRN source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, and documentation.
You can either download this source code as a zip or tar.gz file (see
the `ACRN v0.5 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v0.5>`_ or
use Git clone and checkout commands:

.. code-block:: bash

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v0.5

The project's online technical documentation is also tagged to correspond
with a specific release: generated v0.5 documents can be found at
https://projectacrn.github.io/0.5/.  Documentation for the latest
(master) branch is found at https://projectacrn.github.io/latest/.


Version 0.5 new features
************************

**OVMF support initial patches merged in ACRN**: 
To support booting Windows as a Guest OS, we are
using Opensource Virtual Machine Firmware (OVMF). 
Initial patches to support OVMF have been merged in ACRN hypervisor. 
More patches for ACRN and patches upstreaming to OVMF work will be continuing.

**UP2 board serial port support**: 
This release enables serial port debugging on UP2 boards during SOS and UOS boot.

**One E2E binary to support all UEFI platform**: 
ACRN can support both ApolloLake (APL) and KabbyLake (KBL) NUCs.
Instead of having separate builds, this release offers community
developers a single end-to-end reference build that supports both
UEFI hardware platforms, configured with a new boot parameter.
See :ref:`getting_started` for more information.

**APL UP2 board with SBL firmware**: With this 0.5 release, ACRN
now supports APL UP2 board with slim Bootloader (SBL) firmware.
Slim Bootloader is a modern, flexible, light-weither, open source 
reference boot loader with key benefits such as being fast, small, 
customizable, and secure. An end-to-end reference build with 
ACRN hypervisor, Clear Linux as SOS, and Clear Linux as UOS has been 
verified on UP2/SBL board. See the :ref:`using-sbl-up2` documentation 
for step-by-step instructions.

**Document updates**: Several new documents have been added in this release, including:

* :ref:`skl-nuc-gpu-passthrough`
* :ref:`acrn-dm_parameters`

- :acrn-issue:`878`  - Virtualization HLD
- :acrn-issue:`892`  - Power Management: VMM control
- :acrn-issue:`894`  - Power Management: S5
- :acrn-issue:`914`  - GPU Passthrough
- :acrn-issue:`1124` - MMU code reshuffle                           
- :acrn-issue:`1179` - RPMB key passing                                 
- :acrn-issue:`1180` - vFastboot release version 0.9                             
- :acrn-issue:`1181` - Integrate enabling Crash OS feature as default in VSBL debugversion           
- :acrn-issue:`1182` - vSBL to support ACPI customization                           
- :acrn-issue:`1240` - [APL][IO Mediator] Enable VHOST_NET & VHOST to accelerate guest networking with virtio_net.
- :acrn-issue:`1284` - [DeviceModel]Enable NHLT table in DM for audio passthrough                               
- :acrn-issue:`1313` - [APL][IO Mediator] Remove unused netmap/vale in virtio-net                                 
- :acrn-issue:`1330` - combine VM creating and ioreq shared page setup                                   
- :acrn-issue:`1364` - [APL][IO Mediator] virtio code reshuffle                           
- :acrn-issue:`1496` - provide a string convert api and remove banned function for virtio-blk                                  
- :acrn-issue:`1546` - hv: timer: add debug information for add_timer                      
- :acrn-issue:`1579` - vSBL to Support Ramoops                           
- :acrn-issue:`1580` - vSBL to support crash mode with vFastboot                              
- :acrn-issue:`1626` - support x2APIC mode for ACRN guests                              
- :acrn-issue:`1672` - L1TF mitigation                            
- :acrn-issue:`1747` - Replace function like macro with inline function                               
- :acrn-issue:`1821` - Optimize IO request path  
- :acrn-issue:`1832` - Add OVMF booting support for booting as an alternative to vSBL.
- :acrn-issue:`1882` - Extend the SOS CMA range from 64M to 128M                                 
- :acrn-issue:`1995` - Support SBL firmware as boot loader on Apollo Lake UP2.
- :acrn-issue:`2011` - support DISCARD command for virtio-blk                                   
- :acrn-issue:`2036` - Update and complete `acrn-dm` parameters description in the user guide and HLD
- :acrn-issue:`2037` - Set correct name for each pthread in DM                                  
- :acrn-issue:`2079` - Replace banned API with permitted API function in a crn device-model                                  
- :acrn-issue:`2120` - Optimize trusty logic to meet MISRA-C rules                             
- :acrn-issue:`2145` - Reuse linux common virtio header file for virtio                                                   
- :acrn-issue:`2170` -  For UEFI based hardware platforms, one ClearLinux E2E build binary can be used for all platform's installation 
- :acrn-issue:`2187` - Complete the cleanup of unbounded APIs usage 

Fixed Issues
************

- :acrn-issue:`1986` - UOS will hang once watchdog reset triggered
- :acrn-issue:`1987` - UOS will have same MAC address after launching UOS with virio-net
- :acrn-issue:`2000` - After launching UOS with Audio pass-through, Device (I2C0) doesn't exist in UOS DSDT.dsl
- :acrn-issue:`2030` - UP2 fails to boot with uart=disabled for hypervisor
- :acrn-issue:`2031` - UP2 serial port has no output
- :acrn-issue:`2133` - The system will hang up and print some error info after boot UOS

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

:acrn-issue:`2267` - [APLUP2][LaaG]LaaG can't detect 4k monitor 
   After launching UOS on APL UP2 , 4k monitor cannot be detected.

   **Impact:** UOS hasn't display with 4k monitor.

   **Workaround:** None.

:acrn-issue:`2276` - OVMF failed to launch UOS on UP2.
   UP2 failed to launch UOS using OVMF as virtual bootloader with acrn-dm. 

   **Impact:** UOS cannot boot up using OVMF

   **Workaround:** Use VSBL as virtual bootloader

:acrn-issue:`2277` - [APLNUC]Launch UOS with 5G memory will hang 2 minutes
   If launching UOS with 5G memory, there will be about 2 minutes hang.

   **Impact:** Low UOS boot time performance.

   **Workaround:** None.

:acrn-issue:`2278` - [KBLNUC] Cx/Px is not supported on KBLNUC
   C states/P states is not supported on KBL NUC.

   **Impact:** Power Management states related operations cannot be using in SOS/UOS on KBLNUC

   **Workaround:** None 

:acrn-issue:`2279` - [APLNUC]After exiting UOS with mediator Usb_KeyBoard and Mouse, SOS cannot use the 
   Usb_KeyBoard and Mouse
   After exiting UOS with mediator Usb_KeyBoard and Mouse, SOS cannot use the Usb_KeyBoard and Mouse.
   Reproduce Steps as below:

   1) Insert USB keyboard and mouse in standard A port（USB3.0 port）

   2) Boot UOS by sharing the USB keyboard and mouse in cmd line:
   -s n,xhci,1-1:1-2:1-3:1-4:2-1:2-2:2-3:2-4 \

   3) UOS access USB keyboard and mouse.

   4) Exit UOS.

   5) SOS access USB keyboard and mouse. 

   **Impact:** SOS cannot use USB keyboard and mouse in such case.

   **Workaround:** Unplug and plug-in the USB keyboard and mouse after exiting UOS.

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

These commits have been added to the acrn-hypervisor repo since the v0.4
release in Dec 2018 (click on the CommitID link to see details):

.. comment

   This list is obtained from the command:
   git log --pretty=format:'- :acrn-commit:`%h` %s' --after="2018-03-01"

- :acrn-commit:`b7fda274` config: fix no serial output with SBL on UP2
- :acrn-commit:`ddf1c923` hv: fix violations in md.c md.h and md_internal.h for crypto lib
- :acrn-commit:`c230a1a6` hv: fix violations in sha256.c for crypto lib
- :acrn-commit:`488e7b2a` hv: fix violations in hkdf.c and crypto_api.c for crypto lib
- :acrn-commit:`08843973` HV: cyclomatic complexity 20 in vlapic_icrlo_write_handler
- :acrn-commit:`eaa0e307` HV: remove multiple exit/return in routines in the file of vlapic.c
- :acrn-commit:`8e00180c` HV: Remove goto statement in guest.c
- :acrn-commit:`2e01b4c8` HV: trivial changes to meet MISRA-C
- :acrn-commit:`971eb84c` HV: add const qualifier for functions' arguments in vlapic.c
- :acrn-commit:`63eecf08` HV: remove multiple return statement in get_vcpu_paging_mode() routine
- :acrn-commit:`b4b9ac59` HV: remove few return statement in while loop of copy_gva function
- :acrn-commit:`5a583fb8` HV: move global variable into the scope of calling function
- :acrn-commit:`235eaf05` HV: APICBASE_RESERVED definition is not used by any code. Just remove it
- :acrn-commit:`04d9f52f` update acrn-dm comment, remove the series of dot
- :acrn-commit:`7a930d88` hv: virq: refine hypervisor/arch/x86/virq.c
- :acrn-commit:`7ebc4877` HV: refine cmdline code, move parts into dbg_cmd
- :acrn-commit:`a5ca305c` HV: add API to change vuart base & irq config
- :acrn-commit:`f4beaf50` HV: support vuart base & irq can be changed
- :acrn-commit:`537adaeb` HV: cleanup CONFIG_COM_IRQ related code
- :acrn-commit:`fde0bcc1` HV: disable vuart when dbg uart is disabled
- :acrn-commit:`860c444c` hv: coding style: add `const` qualifier for some function
- :acrn-commit:`6f0edfc3` hv: coding style: use the defined data type __packed
- :acrn-commit:`40f6a9fd` dm: allow PM1_RTC_EN to be written to PM1A
- :acrn-commit:`57c661c4` dm: vrtc: add RTC to ACPI DSDT
- :acrn-commit:`067273af` hv: assign: fix remaining MISRA-C violations
- :acrn-commit:`1dfd05cd` hv: fix mis-usage of "PAGE_SHIFT"
- :acrn-commit:`5c6fe01c` hv:Change pcpu_active_bitmap to static
- :acrn-commit:`682824de` hv:Change phys_cpu_num to static
- :acrn-commit:`59e2de48` dm: acpi: add PSDS table in ACPI table
- :acrn-commit:`90fd5d58` script: fix launch_uos script issue due to unseen character
- :acrn-commit:`96800093` doc: update footer copyright year
- :acrn-commit:`9c27ed10` profiling: fix the profiling tool crash by page faults
- :acrn-commit:`a177d75e` doc: initial draft of ACRN coding guidelines
- :acrn-commit:`d89ce8ae` hv: schedule: fix "Procedure has more than one exit point"
- :acrn-commit:`952943c3` hv: decouple IO completion polling from idle thread
- :acrn-commit:`a0154223` hv: clear NEED_RESCHEDULE flag in schedule
- :acrn-commit:`e8ac9767` hv: use asm_pause() to replace inline ASM to satisfy MISRAC
- :acrn-commit:`329ea42d` dm: fix the memory leak in virtio mei
- :acrn-commit:`8f57c61d` dm: Add teardown callback for mevent in uart_core
- :acrn-commit:`72d1fa50` dm: refine the uart_core
- :acrn-commit:`21aa1907` hv: vcpuid: cpuid leaf 07h has subleaf
- :acrn-commit:`2d3f510d` hv: trusty_hypercall: fix "Procedure has more than one exit point"
- :acrn-commit:`5aa7e29f` hv: hypercall: fix "Procedure has more than one exit point"
- :acrn-commit:`d6a22682` hv: hypercall: fix complicated violations of "Procedure has more than one exit point"
- :acrn-commit:`f680ed5d` hv: hypercall: fix simple violations of "Procedure has more than one exit point"
- :acrn-commit:`5ebaaaf9` doc: add CSS for non-compliant code examples
- :acrn-commit:`e5c12a64` Makefile: add install-samples-up2
- :acrn-commit:`83034b71` Makefile: specify BOARD&FIRMWARE in sb-hypervisor-install
- :acrn-commit:`c932faa2` Makefile: eliminate mistakes due to deprecated PLATFORM
- :acrn-commit:`55691aed` hv: fix coding style violations in mmu.c
- :acrn-commit:`c1fc7f5f` hv: remove the usage of 'atoi()'
- :acrn-commit:`536ce5fb` dm: remove unnecessary ioreq status changing from DM
- :acrn-commit:`2d1ddd88` dm: Add vm_clear_ioreq to clear ioreq status
- :acrn-commit:`1274fca0` HV: x86: Fix "Variable should be declared static"
- :acrn-commit:`b3c199d0` hv: mmio_read: add `const` qualifier
- :acrn-commit:`1dee629e` hv:vtd: fix additional violations in vtd.c
- :acrn-commit:`3998c977` HV: [v2] bugfix in 'hv_access_memory_region_update()'
- :acrn-commit:`59c61403` dm: use snprintf to replace sprintf
- :acrn-commit:`4b3ebf69` dm: use strncpy to replace strcpy
- :acrn-commit:`b3ad44d4` dm: use strnlen to replace strlen
- :acrn-commit:`3e0b06cf` dm: Fix some issues from string operations
- :acrn-commit:`20d0e666` hv: fix sprintf and hypercall violations
- :acrn-commit:`277c7ec8` hv: hypercall: fix "Procedure has more than one exit point"
- :acrn-commit:`7016244c` hv: io: fix MISRA-C violations related to break
- :acrn-commit:`68643b61` hv: vcpuid: leaf 0dh is percpu related
- :acrn-commit:`ea672c5b` hv: update coding style for tampoline.c
- :acrn-commit:`b89b1228` hv: virq: refine acrn_handle_pending_request() has more than one exit point
- :acrn-commit:`e692d4c7` hv: virq: refine acrn_handle_pending_request() use goto instruction
- :acrn-commit:`b4de4d1b` Makefile: add RELEASE variable to make command
- :acrn-commit:`31487e82` Makefile: keep files used for debug target
- :acrn-commit:`ef03385f` hv: Write Buffer Flush - VT-d
- :acrn-commit:`a5113d92` hv: remove duplicated `is_vmx_disabled`
- :acrn-commit:`1b37ed50` hv: vmcall: fix "goto detected" violations
- :acrn-commit:`f6ae8351` dm: flush the input/output during tty open.
- :acrn-commit:`88a7d8b2` hv: virq: refine vcpu_inject_hi_exception()
- :acrn-commit:`3bfa6955` hv: virq: refine vcpu_inject_vlapic_int() has more than one exit point
- :acrn-commit:`9c97f6be` Documentation: split the build instructions into its own guide
- :acrn-commit:`c358d29c` doc: fix vhm_request doxygen comment
- :acrn-commit:`01bc8e56` Documentation: fix formatting in partition mode tutorial
- :acrn-commit:`c3250030` hv: vcpuid: remove unnecessary check code
- :acrn-commit:`83f32c93` hv: vcpuid: leaf 02h has no subleaf, delete un-needed code.
- :acrn-commit:`44bee516` dm: virtio: fix compile issue on ubuntu
- :acrn-commit:`9fe282f0` hv: Makefile: remove unused MACRO
- :acrn-commit:`cf47f6cf` hv: coding style: refine the remaining functions to one exit point
- :acrn-commit:`36dcb0f6` hv: lib: refine inline assembly use in bitmap operation
- :acrn-commit:`ddd07b95` hv: cpu_state_tbl: fix multiple exits
- :acrn-commit:`eb77e25f` hv: ept: fix MISRA-C violations
- :acrn-commit:`5253ac7a` dm: virtio: refine header file
- :acrn-commit:`738f2536` hv: coding style: refine cpu related function to one exit
- :acrn-commit:`9672538c` init: move init_scheduler into cpu.c
- :acrn-commit:`ff0703dd` scheduler: make scheduling based on struct sched_object
- :acrn-commit:`8aae0dff` scheduler: refine make_reschedule_request
- :acrn-commit:`6d673648` scheduler: refine runqueue related functions
- :acrn-commit:`93e588bc` hv: fix e820.c violations
- :acrn-commit:`60f78e1e` hv:vtd: fix MISRA-C violations on procedure has more than one exit point
- :acrn-commit:`a98a1a69` hv:vtd: fix MISRA-C violations on pointer not checked for null before use
- :acrn-commit:`725e1921` hv:vtd: fix MISRA-C violations on comment possibly contains code
- :acrn-commit:`897ffa27` hv:vtd: fix MISRA-C violations on logical conjunctions need brackets
- :acrn-commit:`80b392a8` hv:vtd: fix MISRA-C violations on pointer param should be declared pointer to const
- :acrn-commit:`5282fa89` hv:vtd: fix MISRA-C violations on scope of variable could be reduced
- :acrn-commit:`bec21d14` Patch for modularizing ioapic.[c/h] and related files.
- :acrn-commit:`af9b7476` doc: fix formatting in NUC GSG
- :acrn-commit:`61f03dae` DOC: change PCI uart description from mmio to bdf
- :acrn-commit:`50f5b0f6` hv: vmexit: fix MISRA-C violations related to multiple exits
- :acrn-commit:`0a713e6f` hv: coding style: refine set_vcpuid_entries to one exit
- :acrn-commit:`a56abee9` hv: coding style: refine find_vcpuid_entry
- :acrn-commit:`58d2a418` HV: fix pm code for multi-exits & unsigned const
- :acrn-commit:`97a73951` dm: pass mac seed not to use vm name on UP2
- :acrn-commit:`1c99a975` hv: coding style: refine trusty
- :acrn-commit:`1dca17cd` hv: coding style: refine initialize_trusty to one exit
- :acrn-commit:`8a55f038` hv: coding style: refine hcall_initialize_trusty to one exit
- :acrn-commit:`1d1d2434` DM USB: xHCI: change log level of some logs for S3 online debugging
- :acrn-commit:`5f0c093e` hv: coding style: remove no real declaration for external variable
- :acrn-commit:`1e3358fd` Debug: Add one hypercall to query hardware info
- :acrn-commit:`81a9de60` hv:fix MISRA-C violations in create_vm
- :acrn-commit:`bb47184f` hv: fix enable_msr_interception() function
- :acrn-commit:`56af4332` hv: io: fix MISRA-C violations related to multiple exits
- :acrn-commit:`c03bad1f` hv: io: fix MISRA-C violations related to style
- :acrn-commit:`f27aa70f` hv: coding style: refine page related
- :acrn-commit:`7c2198c4` hv: config.h fix "Nested comment found."
- :acrn-commit:`e22b35e3` HV/DM: Unify the usage of aligned for structure definition with alignment
- :acrn-commit:`71a80d2d` hv: assign: change ptirq vpin source type from enum to macro
- :acrn-commit:`d5865632` hv: assign: remove added ptirq entries if fails to add all
- :acrn-commit:`d48dc387` hv: assign: fix MISRA-C violations on multiple exits
- :acrn-commit:`e8b3e44f` hv: assign: fix MISRA-C violations on potential null pointer deference
- :acrn-commit:`e19dcf57` hv: assign: fix MISRA-C violations on implicit type conversion
- :acrn-commit:`714814f9` hv: move `atoi` and `strtol_dec` to debug directory
- :acrn-commit:`32d6aa97` hv: string: fix MISRA-C violations related to style
- :acrn-commit:`2c6c383e` hv: string: fix MISRA-C violations related to break
- :acrn-commit:`b319e654` HV: fix bug adapt uart mmio to bdf for HV cmdline
- :acrn-commit:`23c2166a` HV: change serial PCI cfg to bus:dev.func format
- :acrn-commit:`1caf58f2` hv:clean io_request.c misra violations
- :acrn-commit:`530388db` hv: irq: fix MISRA-C violations in irq.c and idt.h
- :acrn-commit:`08cf8f64` hv: lapic: fix MISRA-C violation of potential numeric overflow
- :acrn-commit:`83ebd432` hv: ptdev: fix MISRA-C violations
- :acrn-commit:`ccda4595` dm: passthru: add error handling if msix table init failed
- :acrn-commit:`3363779d` dm: passthru: msi/msix handling revisit
- :acrn-commit:`38c11784` hv: coding style: refine mmu.c
- :acrn-commit:`2fefff34` HV: x86: fix "Global variable should be declared const"
- :acrn-commit:`eff94591` HV: x86: fix "Procedure has more than one exit point"
- :acrn-commit:`e283e774` hv: vmcs: fix MISRA-C violations related to multiple exits
- :acrn-commit:`4618a6b1` hv: vmcs: fix MISRA-C violations related to pointer
- :acrn-commit:`8e58a686` hv: vmcs: fix MISRA-C violations related to variable scope
- :acrn-commit:`9a051e7a` hv: vmcs: fix MISRA-C violations related to style
- :acrn-commit:`7d8cd911` security: remove gcc flags Wformat Wformat-security in HV
- :acrn-commit:`d133f95d` hv: fix MISRA-C violations "Pointer param should be declared pointer to const."
- :acrn-commit:`f81fb21a` HV: modularization to refine pm related code.
- :acrn-commit:`03262a96` hv: refine coding style for ucode.c
- :acrn-commit:`927c5172` hv: vpci: fix MISRA-C violations related to variable declarations
- :acrn-commit:`4c28e98d` hv: refine a few functions to only one exit point
- :acrn-commit:`64a46300` hv:refine prepare_vm0 api
- :acrn-commit:`b5e0efca` hv: coding style: refine memory.c
- :acrn-commit:`5b467269` hv: lib: remove memchr
- :acrn-commit:`97132acc` Make ibrs_type as internal variable
- :acrn-commit:`55cce7e4` Fix MISRA-C violation in cpu_caps.c and security.c
- :acrn-commit:`689c1c28` function name change in init.c
- :acrn-commit:`5968da46` move security related funcs into security.c
- :acrn-commit:`0ad6da99` make detect_cpu_cap as internal function
- :acrn-commit:`e22217fd` refine apicv capability check
- :acrn-commit:`7c8b7671` refine in cpu_caps.c
- :acrn-commit:`63773db4` change get_monitor_cap to has_monitor_cap
- :acrn-commit:`6830619d` modularization: combine vmx_caps into cpu_caps
- :acrn-commit:`746fbe14` modularization: move functions related with cpu caps into cpu_caps.c
- :acrn-commit:`b8ffac8b` hv:fix possible buffer overflow in 'ptirq_get_intr_data()'
- :acrn-commit:`6aa42272` fix "Procedure has more than one exit point."
- :acrn-commit:`65a7be8f` hv:refine alloc_vm_id api
- :acrn-commit:`235ad0ff` hv: refine memcpy_s
- :acrn-commit:`f9897c6f` hv: refine memset
- :acrn-commit:`78e9a84f` hv: add fast string enhanced rep movsb/stosb check on initial
- :acrn-commit:`3515ca1e` hv: vpci: fix "Procedure has more than one exit point"
- :acrn-commit:`c547e9cf` hv: enable/disable snoop control bit per vm
- :acrn-commit:`20280341` hv: MISRA-C fix "identifier reuse" in vpci code
- :acrn-commit:`2ddd24e0` dm: storage: support discard command
- :acrn-commit:`f71370ad` dm: storage: rename delete to discard
- :acrn-commit:`36863a0b` modularization: vmx on/off should not use vcpu param
- :acrn-commit:`bed82dd3` cleanup vmcs source and header files
- :acrn-commit:`731c4836` modularization: separate vmx.c into two parts
- :acrn-commit:`0d5c65f1` hv: enforce data size on all out exits
- :acrn-commit:`5ab68eb9` dm: hw: Replace sscanf with permitted string API
- :acrn-commit:`63b814e7` dm: hw: Replace strlen with strnlen
- :acrn-commit:`eab7cd47` dm: hw: Replace sprintf with snprintf
- :acrn-commit:`69dc9392` hv: drop the temporary stack for AP startup
- :acrn-commit:`74849cd9` modularization:move out efi dir from hypervisor
- :acrn-commit:`59e3f562` remove check_tsc
- :acrn-commit:`d2bac7cc` cpu_dead should only run on current pcpu
- :acrn-commit:`d2627ecf` DM USB: xHCI: fix an issues for failing to enumerate device
- :acrn-commit:`1c3344b7` DM USB: xHCI: change log level for S3 process
- :acrn-commit:`3dadb62d` HV: fix bug change default vuart IRQ for UP2 board
- :acrn-commit:`a3d2a7e7` hv: vpci: 2 MISRA-C violation fixes
- :acrn-commit:`44e9318c` hv: vmsr: fix MISRA_C violations
- :acrn-commit:`117b71e6` doc: add partition mode hld
- :acrn-commit:`ed5e210d` Doc: Update GSG for v0.4 version and launch and acrn.conf sample script
- :acrn-commit:`57bf26dc` hv: fix possible buffer overflow issues
- :acrn-commit:`73ab7274` dm: set correct thread name
- :acrn-commit:`cb313815` dm: vhost: remove support for non-msix devices
- :acrn-commit:`b29fc619` dm: virtio-net: apply new mevent API to avoid race issue
- :acrn-commit:`4f36244f` dm: virtio-console: apply new mevent API to avoid race issue
- :acrn-commit:`baf8f8bd` dm: virtio-input: apply new mevent API to avoid race issue
- :acrn-commit:`c2df4a85` DM USB: xHCI: no wait logic implementation for S3
- :acrn-commit:`82659831` DM USB: xHCI: refine emulation for command XHCI_CMD_RS
- :acrn-commit:`e5c98e6d` DM USB: add usb_dev_path_cmp function for convenience
- :acrn-commit:`6c1ca137` DM USB: xHCI: remove the waiting 5 seconds wa for s3
- :acrn-commit:`4fc5dcfc` hv: enable SMAP in hypervisor
- :acrn-commit:`57dfc7de` hv: refine IOREQ state operation functions in hypervisor
- :acrn-commit:`c89d6e65` modularization: clean up namings in vMTRR module
- :acrn-commit:`6bbd0129` modularization: move vMTRR code to guest directory
- :acrn-commit:`e066774a` hv: refine strnlen_s/strstr_s to only one exit point
- :acrn-commit:`e114ea7e` hv: timer: fix procedure has more than one exit point
- :acrn-commit:`4131d46f` hv: remove goto in ept_violation_vmexit_handler
- :acrn-commit:`a958fea7` hv: emulate IA32_TSC_ADJUST MSR
- :acrn-commit:`6b998580` Fix KW issues for tpm_emulator
- :acrn-commit:`2d469a5e` modularization: hypervisor initialization component
- :acrn-commit:`9a7d32f0` modularization: reorg the bsp_boot_init & cpu_secondary_init
- :acrn-commit:`9e917057` profiling: split profiling_vmexit_handler into two functions
- :acrn-commit:`302494cb` doc: update some statements
- :acrn-commit:`07309fdc` doc: update some statements
- :acrn-commit:`40f375b4` Doc: modify the note of UOS kernel modules
- :acrn-commit:`2d9e478c` Doc: delete the step of downloading UOS's kernel
- :acrn-commit:`c3a4a5d4` Doc: add "$" for code
- :acrn-commit:`d56e2c29` Doc: update the steps
- :acrn-commit:`2be939f3` Doc: add "Deploy the UOS kernel modules for AGL"
- :acrn-commit:`73161f91` Update using_agl_as_uos.rst
- :acrn-commit:`c51394c3` doc: update the doc of AGL as UOS
- :acrn-commit:`e5748795` doc: update the doc of using AGL as UOS
- :acrn-commit:`fbaecde6` DM USB: xHCI: Fix banned API issue.
- :acrn-commit:`e835f5f5` dm: enforce data size when accessing PCI BARs
- :acrn-commit:`f5a66e8e` doc: update OVMF usage for acrn-dm
- :acrn-commit:`d8c4e7d3` dm: add option to boot OVMF from acrn-dm
- :acrn-commit:`9e97fd06` dm: add BIOS/ROM image loading support at High BIOS region
- :acrn-commit:`653a5795` dm: query and save image size during initial checking
- :acrn-commit:`a80f08eb` dm: add launch_uos.args sample file for AaaG
- :acrn-commit:`04fef4f3` tools: acrn-manager: change path of vm conf files
- :acrn-commit:`2f30dcdb` hv: refine strncpy_s to only one exit point
- :acrn-commit:`b8ca17c6` hv: remove strcpy_s
- :acrn-commit:`29c8494f` hv: replace strcpy_s with strncpy_s
- :acrn-commit:`07427b4c` modularization: move virtual cpuid stuff into guest dir
- :acrn-commit:`90d7bddd` doc: vertical align table content to top
- :acrn-commit:`e4143ca1` doc: fix use of double dashes
- :acrn-commit:`6dec1667` doc: improve acrn-dm param layout
- :acrn-commit:`21a5b308` Update add acrn-dm parameter descriptions
- :acrn-commit:`c45300fb` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`6d5b769d` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`5998f434` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`c607aedf` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`ba79b218` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`4ab193cf` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`1c70f812` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`341bf84c` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`a0708339` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`a7be8f73` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`5aedc8f4` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`e7e8ce63` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`24542894` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`edd06fe9` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`1ef6b657` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`8b13bf3f` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`7446089d` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`666c97b0` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`a21c3ca3` Update doc/developer-guides/hld/hld-devicemodel.rst
- :acrn-commit:`7bcd7054` doc: additional DM parameter documentation
- :acrn-commit:`97c95697` doc: update code to "losetup -r"
- :acrn-commit:`4355b0df` doc: update some statements
- :acrn-commit:`ad1ba225` doc: update some statements
- :acrn-commit:`6bfbf166` Doc: Update some statements
- :acrn-commit:`85b30685` Doc: define swap partition with 1G
- :acrn-commit:`fae136c2` doc: remove "software-defined-cockpit"
- :acrn-commit:`33b87064` Doc: Update the doc of "Build UOS from ClearLinux"
- :acrn-commit:`8b83cadd` doc: update the layout of the doc
- :acrn-commit:`71bf586e` doc: upload tutorial of 'Build UOS from Clear Linux'
- :acrn-commit:`bc5b27a7` tools: acrnctl: increase STOP_TIMEOUT to 30s when reset VM
- :acrn-commit:`bb768904` config: add up2-sbl board related configs
- :acrn-commit:`59c2b33a` Makefile: separate PLATFORM into BOARD+FIRMWARE
- :acrn-commit:`064a3106` tools: vmcfg: use defconfig instead of default values in Kconfig
- :acrn-commit:`ed1c576d` dm: pass mac seed not to use vm name
- :acrn-commit:`e3fc6c3c` hv: use int32_t replace int
- :acrn-commit:`e8f3a2d4` hv: use uint64_t replace "unsigned long"
- :acrn-commit:`473d8713` hv: use uint32_t replace "unsigned int"
- :acrn-commit:`8bafde99` hv: use uint8_t replace "unsigned char"
- :acrn-commit:`a1435f33` dm: bios: update vSBL to V1.1
- :acrn-commit:`4d13ad9d` hv: enable NX in hypervisor
- :acrn-commit:`405d1335` doc: add 0.4 to doc version menu
- :acrn-commit:`2ef06450` dm: virtio-input: ignore all MSC events from FE
- :acrn-commit:`19fb5fa0` dm: adjust the sequence of destroy client and wait for vm_loop exit
- :acrn-commit:`bff592d9` HV: rename e820_entries to e820_entries_count
- :acrn-commit:`9b58b9d1` HV: improve e820 interfaces and their usages
- :acrn-commit:`b69d24b1` HV: separate e820 related code as e820.c/h
- :acrn-commit:`c5d827ab` ACRN: Add runC container sample config file
- :acrn-commit:`da0cf3af` DM: xHCI: unbind slot id and ndevices relationship.
- :acrn-commit:`c2be20d2` move idt.S and idt.h out of boot component
- :acrn-commit:`27938c33` move idt fixup out of cpu_primary.S
- :acrn-commit:`6b42b347` init fs and gs with 0x10
- :acrn-commit:`cf34cda3` version: 0.5-unstable

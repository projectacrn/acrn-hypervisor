.. _release_notes_1.0:

ACRN v1.0 (May 2019)
####################

We are pleased to announce the release of Project ACRN version 1.0,
a major ACRN project milestone focused on automotive application use.

ACRN is a flexible, lightweight reference hypervisor, built with
real-time and safety-criticality in mind, optimized to streamline embedded 
development through an open source platform. Check out the :ref:`introduction` for more information.
All project ACRN source code is maintained in the https://github.com/projectacrn/acrn-hypervisor 
repository and includes folders for the ACRN hypervisor, the ACRN device 
model, tools, and documentation. You can either download this source code as
a zip or tar.gz file (see the `ACRN v1.0 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v1.0>`_)
or use Git clone and checkout commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v1.0

The project’s online technical documentation is also tagged to correspond 
with a specific release: generated v1.0 documents can be found at https://projectacrn.github.io/1.0/. 
Documentation for the latest (master) branch is found at https://projectacrn.github.io/latest/.
ACRN v1.0 requires Clear Linux OS version 29070 or newer. Please follow the 
instructions in the :ref:`getting-started-apl-nuc`.

Version 1.0 major features
**************************

Hardware Support
================
Apollo Lake NUC, Apollo Lake UP Squared (UP2) board and
Kaby Lake NUC are supported. (See :ref:`hardware` for supported platform details.)

APL UP2 board with SBL firmware
===============================
ACRN supports APL UP2 board with Slim Bootloader (SBL) firmware.
Slim Bootloader is a modern, flexible, light-weight,
open source reference bootloader that is also  fast, small,
customizable, and secure. An end-to-end reference build has been verified 
on UP2/SBL board using ACRN hypervisor, Clear Linux OS as SOS, and Clear
Linux OS as UOS. See the :ref:`using-sbl-up2` for step-by-step instructions.

Enable post-launched RTVM support for real-time UOS in ACRN
===========================================================
This release provides initial patches enabling a User OS (UOS) running as a
virtual machine (VM) with real-time characteristics,
also called a “post-launched RTVM”. We’ve published a tutorial
:ref:`rt_linux_setup`. More patches for ACRN real time support will continue.

Enable cache QOS with CAT
=========================
Cache Allocation Technology (CAT) is available on Apollo Lake (APL) platforms,
providing cache isolation between VMs mainly for real-time performance quality
of service (QoS). The CAT for a specific VM is normally set up at boot time per
the VM configuration determined at build time. For debugging and performance
tuning, the CAT can also be enabled and configured at runtime by writing proper
values to certain MSRs using the ``wrmsr`` command on ACRN shell.

Enable QoS based on runC container
==================================
ACRN supports Device-Model QoS based on runC container to control 
the SOS resources (CPU, Storage, MEM, NET) by modifying the runC configuration file,
configuration guide will be published in next release.

S5 support for RTVM
===================
ACRN supports a Real-time VM (RTVM) shutting itself down. A RTVM is a kind
of VM that the SOS can’t interfere with at runtime, and as such, only the
RTVM can power itself off internally. All power-off requests external to the
RTVM will be rejected to avoid any interference.

OVMF support initial patches merged in ACRN
===========================================
To support booting Windows as a Guest OS, we are using
Open source Virtual Machine Firmware (OVMF). Initial
patches to support OVMF have been merged in ACRN hypervisor. More patches for
ACRN and patches upstreaming to OVMF work will be continuing.

Support ACPI power key mediator
===============================
ACRN supports ACPI power/sleep key on the APL and KBL NUC platforms, triggering
S3/S5 flow, following the ACPI spec.

VT-x, VT-d
==========
Based on Intel VT-x virtualization technology, ACRN emulates a virtual CPU 
with core partitioning. VT-d provides hardware support
for isolating and restricting device accesses to only the owner of the 
partition managing the device. It allows assigning I/O devices to a VM, 
and extending the protection and isolation properties of VMs for 
I/O operations.

PIC/IOAPIC/MSI/MSI-X/PCI/LAPIC
==============================
ACRN hypervisor supports virtualized APIC-V, EPT, IOAPIC, and LAPIC functionality.

Ethernet
========
ACRN hypervisor supports virtualized Ethernet functionality. The Ethernet 
Mediator is executed in the Service OS and provides packet forwarding between 
the physical networking devices (Ethernet, Wi-Fi, etc.) and virtual devices in
the Guest VMs (also called “User OS”). The HW platform physical connection can be shared
with Linux, Android by Service OS guests for regular (i.e. non-AVB) traffic.

Mass Storage
============
ACRN hypervisor supports virtualized non-volatile R/W storage for the Service 
OS and Guest OS instances, supporting VM private storage and/or storage shared 
between Guest OS instances.

USB (xDCI)
==========
ACRN hypervisor supports pass-through of xDCI 
controllers to a Guest OS from the platform.

USB Mediator (xHCI)
===================
ACRN hypervisor supports an emulated USB xHCI controller for Guest OS.
(See :ref:`usb_virtualization` for more information.)

Wi-Fi
=====
ACRN hypervisor supports doing a pass-through of the Wi-Fi chip to
a Guest VM (UOS), enables control of the Wi-Fi as an in-vehicle hotspot for third-party
devices, provides third-party device applications access to the vehicle, and
provides access of third-party devices to the TCU (if applicable) provided connectivity.

IPU (MIPI CSI-2, HDMI-in)
=========================
ACRN hypervisor provide an IPU mediator to share with Guest OS. Altenatively, IPU
can also be configured as pass-through to Guest OS without sharing.

Bluetooth
=========
ACRN hypervisor supports Bluetooth controller passthrough to a single Guest
OS (for example, for In-Vehicle Infotainment or IVI use cases).

GVT-g for ACRN
==============
GVT-g for ACRN (a.k.a AcrnGT) is a feature to enable GPU sharing Service OS 
and User OS, so both can run GPU workload simultaneously. Direct display is 
supported by AcrnGT, where the Service OS and User OS are each assigned to 
a different display. The display ports support eDP and HDMI. See :ref:`APL_GVT-g-hld`
for more information.

GPU - Preemption
================
GPU Preemption is one typical automotive use case which requires the system 
to preempt GPU resources occupied by lower priority workloads. This is done 
to ensure performance of the most critical workload can be achieved. Three 
different schedulers for the GPU are involved: i915 UOS scheduler, Mediator 
GVT scheduler, and i915 SOS scheduler.

GPU - display surface sharing via Hyper DMA
===========================================
Surface sharing is one typical automotive use case which requires that the 
SOS accesses an individual surface or a set of surfaces from the UOS without 
having to access the entire frame buffer of the UOS. It leverages hyper_DMABUF,
a Linux kernel driver running on multiple VMs and expands DMA-BUFFER sharing
capability to inter-VM.

Virtio standard is supported
============================
Virtio framework is widely used in ACRN, allowing devices beyond network and
storage to be shared to UOS in a standard way. Many mediators in ACRN follow
the virtio spec. Virtio-based virtualization is called para-virtualization.
Virtio is a virtualization standard for network and disk device drivers where 
the guest’s device driver “knows” it is running in a virtual environment, and 
cooperates with the hypervisor. The SOS and UOS can share physical LAN network 
and physical eMMC storage device. (See :ref:`virtio-hld` for more information.)

Device pass-through support
===========================
Device pass-through to UOS supported with help of VT-d.

GPIO virtualization
===================
GPIO virtualization is supported as para-virtualization based on the Virtual 
I/O Device (VIRTIO) specification. The GPIO consumers of the Front-end are able
to set or get GPIO values, directions, and configuration via one virtual GPIO 
controller. In the Back-end, the GPIO command line in the launch script can be 
modified to map native GPIO to UOS. (See :ref:`virtio-hld` for more information.)

New ACRN tools
==============
We’ve added a collection of support tools including ``acrnctl``, ``acrntrace``, ``acrnlog``,
``acrn-crashlog``, ``acrnprobe``. (See :ref:`tools` for details.)

Document updates
================
We have many reference documents `available
<https://projectacrn.github.io>`_, including:

* :ref:`GPU Passthrough on Skylake NUC <skl-nuc-gpu-passthrough>`
* :ref:`Device Model Parameters <acrn-dm_parameters>`
* :ref:`Running Automotive Grade Linux as a VM <agl-vms>`
* :ref:`Using PREEMPT_RT-Linux for real-time UOS <rt_linux_setup>`
* :ref:`Frequently Asked Questions <faq>`
* :ref:`An introduction to Trusty and Security services on ACRN <trusty-security-services>`
* A Wiki article about `Porting ClearLinux/ACRN to support Yocto/ACRN
  <https://github.com/projectacrn/acrn-hypervisor/wiki/Yocto-based-Service-OS-(SOS)-and-User-OS-(UOS)-on-ACRN>`_
* An `ACRN brochure update (English and Chinese)
  <https://projectacrn.org/#code-docs>`_
* ACRN Roadmap: look ahead in `2019
  <https://projectacrn.org/wp-content/uploads/sites/59/2019/02/ACRN-Rodamap-2019.pdf>`_
* Performance analysis of `VBS-k framework
  <https://projectacrn.github.io/latest/developer-guides/VBSK-analysis.html>`_
* HLD design doc for `IOC virtualization
  <https://projectacrn.github.io/latest/developer-guides/hld/hld-APL_GVT-g.html?highlight=hld>`_
* Additional project `coding guidelines
  <coding_guidelines.html>`_
* :ref:`Zephyr RTOS as Guest OS <using_zephyr_as_uos>`
* :ref:`Enable cache QoS with CAT <using_cat_up2>`
* :ref:`ACRN kernel parameter introduction <kernel-parameters>`
* :ref:`FAQ update for two issues <faq>`
* :ref:`ACRN Debug introduction <acrn-debug>`
* :ref:`Converged Security Engine (CSE) <hld-security>`

New Features Details
********************

- :acrn-issue:`866` - Security Interrupt Storm Mitigation
- :acrn-issue:`887` - Security xD support
- :acrn-issue:`888` - Security: Service OS Support for Platform Security Discovery
- :acrn-issue:`892` - Power Management: VMM control
- :acrn-issue:`894` - Power Management: S5
- :acrn-issue:`914` - GPU Passthrough
- :acrn-issue:`940` - Device: IPU support
- :acrn-issue:`951` - Device CS(M)E support
- :acrn-issue:`1122` - Security Enable compiler and linker setting-flags to harden software
- :acrn-issue:`1179` - RPMB key passing
- :acrn-issue:`1180` - vFastboot release version 0.9
- :acrn-issue:`1181` - Integrate enabling Crash OS feature as default in VSBL debug Version
- :acrn-issue:`1182` - vSBL to support ACPI customization
- :acrn-issue:`1240` - [APL][IO Mediator] Enable VHOST_NET & VHOST to accelerate guest networking with virtio_net.
- :acrn-issue:`1284` - [DeviceModel]Enable NHLT table in DM for audio passthrough
- :acrn-issue:`1329` - ioeventfd and irqfd implementation to support vhost on ACRN
- :acrn-issue:`1343` - Enable -Werror for ACRN hypervisor
- :acrn-issue:`1409` - Add support for profiling [sep/socwatch tools]
- :acrn-issue:`1455` - x2apic support for acrn
- :acrn-issue:`1498` - add watchdog MSI and INTR support in DM
- :acrn-issue:`1536` - dm: add virtio_mei mediator
- :acrn-issue:`1544` - dm: rpmb: Support RPMB mode config from launch.sh
- :acrn-issue:`1568` - Implement PCI emulation functionality in HV for UOS passthrough devices and SOS MSI/MSI-X remapping
- :acrn-issue:`1626` - support x2APIC mode for ACRN guests
- :acrn-issue:`1672` - L1TF mitigation
- :acrn-issue:`1701` - MISRA C compliance Naming Convention
- :acrn-issue:`1711` - msix.c use MMIO read/write APIs to access MMIO registers
- :acrn-issue:`1812` - export kdf_sha256 interface from crypto lib
- :acrn-issue:`1815` - improve emulation of I/O port CF9
- :acrn-issue:`1824` - implement “wbinvd” emulation
- :acrn-issue:`1832` - Add OVMF booting support for booting as an alternative to vSBL.
- :acrn-issue:`1882` - Extend the SOS CMA range from 64M to 128M
- :acrn-issue:`1915` - Enable Audio Mediator
- :acrn-issue:`1953` - Add cmdline option to disable/enable vhm module for guest
- :acrn-issue:`1995` - Support SBL firmware as boot loader on Apollo Lake UP2.
- :acrn-issue:`2011` - support DISCARD command for virtio-blk
- :acrn-issue:`2020` - DM: Enable QoS in ACRN, based on runC container
- :acrn-issue:`2056` - Enable SMAP in hypervisor
- :acrn-issue:`2170` - For UEFI based hardware platforms, one Clear Linux OS E2E build binary can be used for all platform’s installation
- :acrn-issue:`2176` - Fix RTC issues in ACPI
- :acrn-issue:`2319` - Add vHPET support
- :acrn-issue:`2351` - Enable post-launched hybrid mode
- :acrn-issue:`2426` - Enable Interrupt Remapping feature
- :acrn-issue:`2431` - VPCI code cleanup
- :acrn-issue:`2448` - Adding support for socket as a backend for Virtio-Console
- :acrn-issue:`2462` - Enable cache QOS with CAT
- :acrn-issue:`2512` - GPIO virtualization
- :acrn-issue:`2708` - one binary for SBL and UEFI
- :acrn-issue:`2713` - Enable ACRN to boot Zephyr
- :acrn-issue:`2792` - Pass ACRN E820 map to OVMF
- :acrn-issue:`2865` - support S5 of Normal VM with lapic_pt

Fixed Issues Details
********************

- :acrn-issue:`424` - Clear Linux OS desktop GUI of SOS fails to launch
- :acrn-issue:`663` - Black screen displayed after booting SOS/UOS
- :acrn-issue:`676` - Hypervisor and DM version numbers incorrect
- :acrn-issue:`677` - SSD Disk ID is not consistent between SOS/UOS
- :acrn-issue:`706` - Invisible mouse cursor in UOS
- :acrn-issue:`707` - Issues found with instructions for using Ubuntu as SOS
- :acrn-issue:`721` - DM for IPU mediation
- :acrn-issue:`843` - ACRN boot failure
- :acrn-issue:`971` - acrncrashlog functions need to be enhance
- :acrn-issue:`1003` - CPU: cpu info is not correct
- :acrn-issue:`1071` - hypervisor cannot boot on skylake i5-6500
- :acrn-issue:`1101` - missing acrn_mngr.h
- :acrn-issue:`1125` - VPCI coding style and bugs fixes found in integration testing for partition mode
- :acrn-issue:`1126` - VPCI coding style and bugs fixes for partition mode
- :acrn-issue:`1209` - specific PCI device failed to passthrough to UOS
- :acrn-issue:`1268` - GPU hangs when running GfxBench Car Chase in SOS and UOS.
- :acrn-issue:`1270` - SOS and UOS play video but don’t display video animation output on monitor.
- :acrn-issue:`1319` - SD card pass-through: UOS can’t see SD card after UOS reboot.
- :acrn-issue:`1339` - SOS failed to boot with SSD+NVMe boot devices on KBL NUC
- :acrn-issue:`1432` - SOS failed boot
- :acrn-issue:`1774` - UOS cannot stop by command: acrnctl stop [vm name] in SOS
- :acrn-issue:`1775` - [APL UP2]ACRN debugging tool` - acrntrace cannot be used in SOS
- :acrn-issue:`1776` - [APL UP2]ACRN debugging tool` - acrnlog cannot be used in SOS
- :acrn-issue:`1777` - After UOS plays video for several minutes, the UOS image will be stagnant
- :acrn-issue:`1778` - MSDK: 1080p H264 video decode fails in UOS
- :acrn-issue:`1779` - gfxbench cannot run in SOS&UOS
- :acrn-issue:`1780` - Some video formats cannot be played in SOS.
- :acrn-issue:`1781` - Can not recognize the SD card in the SOS
- :acrn-issue:`1782` - UOS failed to get ip with the pass-throughed network card
- :acrn-issue:`1792` - System hang and reboot after run “LaaG Forced GPU Reset: subtest error-state-capture-vebox” in UOS
- :acrn-issue:`1794` - After SOS boots up, there’s no output on SOS screen
- :acrn-issue:`1795` - SOS fails to get IP address
- :acrn-issue:`1796` - APL NUC fails to reboot sometimes
- :acrn-issue:`1825` - Need to clear memory region used by UOS before it exit
- :acrn-issue:`1837` - ‘acrnctl list’ shows incomplete VM names
- :acrn-issue:`1986` - UOS will hang once watchdog reset triggered
- :acrn-issue:`1987` - UOS will have same MAC address after launching UOS with virio-net
- :acrn-issue:`1996` - [APLNUC/KBLNUC/APLUP2]There is an error log when using “acrnd&” to boot UOS
- :acrn-issue:`1999` - [APLNUC][KBLNUC][APLUP2]UOS reset fails with acrnctl reset command
- :acrn-issue:`2000` - After launching UOS with Audio pass-through, Device (I2C0) doesn’t exist in UOS DSDT.dsl
- :acrn-issue:`2030` - UP2 fails to boot with uart=disabled for hypervisor
- :acrn-issue:`2031` - UP2 serial port has no output
- :acrn-issue:`2043` - Fix incorrect vm_id captured when sampling PMU data
- :acrn-issue:`2052` - tpm_emulator code reshuffle
- :acrn-issue:`2086` - enable/disable snoop control bit per vm
- :acrn-issue:`2133` - The system will hang up and print some error info after boot UOS
- :acrn-issue:`2157` - Profiling: fix the profiling tool crash by page faults
- :acrn-issue:`2168` - Modify Makefile to save debug files
- :acrn-issue:`2200` - Won’t build using ubuntu 16.04 LTS and binutils 2.26.1
- :acrn-issue:`2237` - Don’t export two dma_bufs for the same importer in sos kernel
- :acrn-issue:`2257` - Profiling code clean up
- :acrn-issue:`2276` - OVMF failed to launch UOS on UP2
- :acrn-issue:`2277` - [APLNUC]Launch UOS with 5G memory will hang 2 minutes
- :acrn-issue:`2298` - Hard codes path to iasl
- :acrn-issue:`2298` - Hardcodes path to iasl
- :acrn-issue:`2316` - Tools don’t respect CFLAGS/LDFLAGS from environment
- :acrn-issue:`2338` - [UP2]Lost 2G memory in SOS when using SBL as bootloader on UP2
- :acrn-issue:`2341` - vm exit trace position is not correct
- :acrn-issue:`2349` - SOS failed boot up with RELOC config enabled.
- :acrn-issue:`2355` - Switch the default up-notification vector from 0xF7 to 0xF3
- :acrn-issue:`2356` - fail to start UOS on the renamed device name of VHM module
- :acrn-issue:`2370` - Doesn’t use parallel make in subbuilds
- :acrn-issue:`2371` - kconfig oldconfig doesn’t work correctly
- :acrn-issue:`2389` - Need to add the dependency of $(LIB_FLAGS)
- :acrn-issue:`2410` - Launch UOS will occur page fault error when use the hypervisor build on Ubuntu
- :acrn-issue:`2422` - [PATCH] profiling: fix the system freeze issue when running profiling tool
- :acrn-issue:`2427` - Remove redundant apicv code from legacy vInterrupt inject path
- :acrn-issue:`2453` - Fix vHPET memory leak on device reset
- :acrn-issue:`2455` - host call stack disappear when dumping
- :acrn-issue:`2474` - Need to capture dropped sample info while profiling
- :acrn-issue:`2490` - systemd virtualization detection doesn’t work
- :acrn-issue:`2516` - [UP2][SBL] System hang with DP monitor connected
- :acrn-issue:`2522` - Start ias in SOS, no display
- :acrn-issue:`2523` - UOS monitor does not display when using ias
- :acrn-issue:`2524` - [UP2][SBL] Launching UOS hang while weston is running in SOS
- :acrn-issue:`2528` - [APLUP2] SBL (built by SBL latest code) failed to boot ACRN hypervisor
- :acrn-issue:`2543` - vLAPIC: DCR not properly initialized
- :acrn-issue:`2548` - [APLNUC/KBLNUC][GVT][SOS/LAAG] Weston fails to play video in SOS and UOS
- :acrn-issue:`2572` - Startup SOS Fails
- :acrn-issue:`2588` - Uninitialized Variable is used in acrn_kernel/drivers/acrn/acrn_trace.c and acrn_hvlog.c
- :acrn-issue:`2597` - Return PIPEDSL from HW register instead of cached memory for Guest VGPU
- :acrn-issue:`2606` - HV crash during running VMM related Hypercall fuzzing test.
- :acrn-issue:`2624` - Loading PCI devices with table_count > CONFIG_MAX_MSIX_TABLE_NUM leads to writing outside of struct.
- :acrn-issue:`2643` - Ethernet pass-through, network card can’t get ip in uos
- :acrn-issue:`2674` - VGPU needs the lock when updating ppggt/ggtt to avoid the race condition
- :acrn-issue:`2695` - UOS powers off or suspend while pressing power key, UOS has no response
- :acrn-issue:`2704` - Possible memory leak issues
- :acrn-issue:`2760` - [UP2]{SBL] make APL-UP2 SBL acrn-hypervisor sos image failed.
- :acrn-issue:`2772` - Enable PCI-E realtek MMC card for UOS
- :acrn-issue:`2780` - [APL_NUC KBL_NUC EFI_UP2]Update clear Linux missing acrn.efi file
- :acrn-issue:`2792` - Pass ACRN E820 map to OVMF
- :acrn-issue:`2829` - The ACRN hypervisor shell interactive help is rather terse
- :acrn-issue:`2830` - Warning when building the hypervisor
- :acrn-issue:`2851` - [APL/KBL/UP2][HV][LaaG]Uos cannot boot when acrnctl add Long_VMName of more than 26
- :acrn-issue:`2870` - Use ‘sha512sum’ for validating all virtual bootloaders

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

-----

:acrn-issue:`2974` - Launching Zephyr RTOS as a real-time UOS takes too long
    Launching Zephyr RTOS as a real-time UOS takes too long

    These steps reproduce the issue:

    1) Build Zephyr image by follow the `guide
       <https://projectacrn.github.io/latest/tutorials/using_zephyr_as_uos.html?highlight=zephyr>`_.
    2) Copy the "Zephyr.img", "OVMF.fd" and "launch_zephyr.sh" to ISD.
    3) execute the launch_zephyr.sh script.

    **Impact:** Launching Zephyr RTOS as a real-time UOS takes too long

    **Workaround:** None

-----

Change Log
**********

These commits have been added to the acrn-hypervisor repo since the v0.8
release in Apr 2019 (click on the CommitID link to see details):

.. comment

   This list is obtained from this git command (update the date to pick up
   changes since the last release):

   git log --pretty=format:'- :acrn-commit:`%h` - %s' --after="2018-03-01"

- :acrn-commit:`bed57dd2` - HV: vuart: enable connect mode for VM
- :acrn-commit:`235d8861` - HV: vuart: enable vuart console for VM
- :acrn-commit:`3c92d7bb` - HV: vuart: refine vuart config
- :acrn-commit:`1234f4f7` - HV: shell: rename sos_console to vm_console
- :acrn-commit:`2362e585` - HV: correct usage of GUEST_FLAG_IO_COMPLETION_POLLING
- :acrn-commit:`578592b5` - vlapic: refine IPI broadcast to support x2APIC mode
- :acrn-commit:`581c0a23` - HV: move AP_MASK to cpu.h
- :acrn-commit:`7b6fe145` - HV: Remove unnecssary indent in pm.c
- :acrn-commit:`a85d11ca` - HV: Add prefix 'p' before 'cpu' to physical cpu related functions
- :acrn-commit:`25741b62` - HV: fix the issue of ACRN_REQUEST_EXCP flag is not cleared.
- :acrn-commit:`28d50f1b` - hv: vlapic: add apic register offset check API
- :acrn-commit:`70dd2544` - hv: vmsr: refine x2apic MSR bitmap setting
- :acrn-commit:`0c347e60` - hv: vlapic: wrap APICv check pending delivery interrupt
- :acrn-commit:`037fffc2` - hv: vlapic: wrap APICv inject interrupt API
- :acrn-commit:`1db8123c` - hv: virq: refine pending event inject coding style
- :acrn-commit:`fde2c07c` - hv: vlapic: minor fix about APICv inject interrupt
- :acrn-commit:`846b5cf6` - hv: vlapic: wrap APICv accept interrupt API
- :acrn-commit:`7852719a` - ACRN: tool: Fix buffer overflow risk in acrnctl
- :acrn-commit:`763d2183` - DM: virtio-gpio: fix array overflow issue
- :acrn-commit:`f3f870b7` - dm: uart: use mevent_add only when it is a tty
- :acrn-commit:`30609565` - dm: fix possible null pointer dereference in pci_gvt_deinit
- :acrn-commit:`f991d179` - hv: fix possible buffer overflow in vlapic.c
- :acrn-commit:`2c13ac74` - hv: vmcs: minor fix about APICv feature setting
- :acrn-commit:`4fc20097` - hv: instr_emul: check the bit 0(w bit) of opcode when necessary
- :acrn-commit:`7ccb44af` - HV: Remove dead loop in stop_cpus
- :acrn-commit:`91c14081` - HV: Reset physical core of lapic_pt vm when shutdown
- :acrn-commit:`e52917f7` - HV: Reshuffle start_cpus and start_cpu
- :acrn-commit:`cfe8637c` - HV: Kconfig: Remove CPU_UP_TIMEOUT
- :acrn-commit:`565f3c72` - HV: Clear DM set guest_flags when shutdown vm
- :acrn-commit:`a3207b2b` - hv: allocate vpid based on vm_id and vcpu_id mapping
- :acrn-commit:`9673f3da` - HV: validate target vm in hypercall
- :acrn-commit:`82181f4c` - HV: remove ifndef on vpci_set_ptdev_intr_info
- :acrn-commit:`aef5a4fd` - hv: free ptdev device IRQs when shutting down VM
- :acrn-commit:`82fa9946` - dm: safely access MMIO hint in MMIO emulation
- :acrn-commit:`4c38ff00` - dm: completely remove enable_bar()/disable_bar() functions
- :acrn-commit:`a718fbe8` - dm: pci: change return type to bool
- :acrn-commit:`887d4168` - hv: check vm state before creating a VM
- :acrn-commit:`fa475540` - hv: seed: fix potential NULL pointer dereferencing
- :acrn-commit:`334c5ae7` - hv: ept: correct EPT mapping gpa check
- :acrn-commit:`aee9f3c6` - hv: reset per cpu sbuf pointers during vcpu reset
- :acrn-commit:`56acaacc` - hv: vlapic: add TPR below threshold implement
- :acrn-commit:`a4c9cb99` - hv:change register_mmio_emulation_handler to void
- :acrn-commit:`f1aa35a2` - doc: add security advisory section in ACRN introduction website
- :acrn-commit:`3e19d62b` - doc: update coding guidelines
- :acrn-commit:`bba43290` - Setting up KBL serial console on the GSG
- :acrn-commit:`0ae5ef3a` - dm: add IOCTL command to get platform information
- :acrn-commit:`5a51d0bf` - hv: Add host CR2 to exception dump
- :acrn-commit:`b1e68453` - hv: enable vMCE from guest CPUID
- :acrn-commit:`35ef11e6` - HV: enable lapic passthru for logical partition VM1
- :acrn-commit:`824caf8c` - hv: Remove need for init_fallback_iommu_domain and fallback_iommu_domain
- :acrn-commit:`948d58fb` - acrn-dm: enable debug option for acrn-dm
- :acrn-commit:`2e5a6e28` - watchdog: map the watchdog reset to warm reset
- :acrn-commit:`2f4e3207` - dm: virtio-input: adapt Windows virtio-input driver
- :acrn-commit:`81158579` - dm: pci: unregister bars which are still enabled in pci_emul_free_bars
- :acrn-commit:`fd389cb1` - dm: disable ACPI PM timer
- :acrn-commit:`98dfc6f2` - dm: virtio-block: extend the max iov number of virtio block
- :acrn-commit:`fa7f6c2c` - dm: fix deadlock between emulate_mem and un/register_mem
- :acrn-commit:`d648df76` - dm: register_bar/unregister_bar when bar enable/disable
- :acrn-commit:`b838e9b7` - dm: pm: mask the higher bits of parameter of smi_cmd handler
- :acrn-commit:`15966f94` - dm: uart: add uart over tcp support
- :acrn-commit:`48be6f1f` - HV:config:Add config to enable logic partition on KBL NUC i7
- :acrn-commit:`c4c788ca` - HV:BSP:Update firmware detection and operations selecting logic
- :acrn-commit:`a13c19b4` - HV:EFI-STUB:UEFI loader name supporting
- :acrn-commit:`048d72fd` - tools: acrn-crashlog: fix some possible memory leak
- :acrn-commit:`46480f6e` - hv: add new hypercall to fetch platform configurations
- :acrn-commit:`e216f306` - tools: acrn-mngr: add delay to allow user to prevent VM autostart for debug
- :acrn-commit:`8c2ab95f` - tools: acrnd: fix wait_for_stop() return wrong vm state
- :acrn-commit:`2b900a43` - tools: acrn-manager: fix mngr_send_msg() return 0 when read ack fail
- :acrn-commit:`6ac9e15a` - dm: fix possible memory leak in 'load_elf32()'
- :acrn-commit:`e50c0c88` - tools: acrn-manager: fix the possibility of creating directory at will by no permission process
- :acrn-commit:`16a2af57` - hv: Build mptable for guest if VM type is Pre-Launched
- :acrn-commit:`869de397` - hv: rename 'assign_iommu_device' and 'unassign_iommu_device'
- :acrn-commit:`ccecd550` - HV: show VM UUID in shell
- :acrn-commit:`445999af` - HV: make vm id statically by uuid
- :acrn-commit:`cb10dc7e` - HV: return bool in sanitize_vm_config
- :acrn-commit:`60712343` - HV: use term of UUID
- :acrn-commit:`4557033a` - hv: vlapic: minor fix about vlapic write
- :acrn-commit:`fa8fa37c` - hv: vlapic: remove vlapic_rdmsr/wrmsr
- :acrn-commit:`ad1bfd95` - hv: move pci.h to include/hw
- :acrn-commit:`69627ad7` - hv: rename io_emul.c to vmx_io.c
- :acrn-commit:`17faa897` - hv:move common/io_req.c/h to dm folder
- :acrn-commit:`2b79c6df` - hv:move some common APIs to io_req.c
- :acrn-commit:`0a1c016d` - hv: move 'emul_pio[]' from strcut vm_arch to acrn_vm
- :acrn-commit:`35c8437b` - hv:move 'fire_vhm_interrupt' to io_emul.c
- :acrn-commit:`e7605fad` - doc: fix misspellings
- :acrn-commit:`c42f5c5c` - Add description of enabling serial console for KBL NUC.
- :acrn-commit:`8ee00c1e` - Update doc/getting-started/gsg_quick_setup.sh
- :acrn-commit:`1312fc6f` - Update doc/getting-started/gsg_quick_setup.sh
- :acrn-commit:`64f74b76` - Update doc/getting-started/gsg_quick_setup.sh
- :acrn-commit:`c3b9b4c1` - Update doc/getting-started/gsg_quick_setup.sh
- :acrn-commit:`f964ee92` - Update doc/getting-started/gsg_quick_setup.sh
- :acrn-commit:`595744a3` - Update doc/getting-started/gsg_quick_setup.sh
- :acrn-commit:`07baa83c` - Update doc/getting-started/gsg_quick_setup.sh
- :acrn-commit:`bf51fb03` - Update doc/getting-started/gsg_quick_setup.sh
- :acrn-commit:`b1adc035` - Update doc/getting-started/gsg_quick_setup.sh
- :acrn-commit:`65ed6c61` - Update doc/getting-started/gsg_quick_setup.sh
- :acrn-commit:`875fc6e8` - Update doc/getting-started/gsg_quick_setup.sh
- :acrn-commit:`a6df7440` - Update doc/getting-started/gsg_quick_setup.sh
- :acrn-commit:`7ff61fb8` - Update doc/getting-started/gsg_quick_setup.sh
- :acrn-commit:`76b34ee7` - Update doc/getting-started/gsg_quick_setup.sh
- :acrn-commit:`a7f7b854` - Add gsg quick setup script.
- :acrn-commit:`122685b7` - DM USB: xHCI: refine the failure process logic of control transfer
- :acrn-commit:`69152647` - hv: Use virtual APIC IDs for Pre-launched VMs
- :acrn-commit:`8796ded2` - DM USB: fix SWWDT_UNHANDLED issue
- :acrn-commit:`8bd7b9be` - DM USB: xHCI: fix an logic error during USB reset
- :acrn-commit:`b570755f` - Domain id and name added to launch_uos.sh
- :acrn-commit:`6eaadc34` - dm: passthru: support SD hotplug
- :acrn-commit:`784bfa28` - DM USB: xHCI: fix an issue during BULK transfer
- :acrn-commit:`e30cd452` - doc: tweak home page redirect to latest
- :acrn-commit:`63743d8b` - DM USB: xHCI: WA for an isochronous crash issue
- :acrn-commit:`f0e7ce6a` - version: 1.0-unstable

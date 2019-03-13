.. _release_notes_0.7:

ACRN v0.7 (Mar 2019)
####################

We are pleased to announce the release of Project ACRN version 0.7.

ACRN is a flexible, lightweight reference hypervisor, built with
real-time and safety-criticality in mind, optimized to streamline
embedded development through an open source platform. Check out the
:ref:`introduction` for more information.


All project ACRN source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, tools, and
documentation.  You can either download this source code as a zip or
tar.gz file (see the `ACRN v0.7 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v0.7>`_ or
use Git clone and checkout commands:

.. code-block:: bash

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v0.7

The project's online technical documentation is also tagged to correspond
with a specific release: generated v0.7 documents can be found at
https://projectacrn.github.io/0.7/.  Documentation for the latest
(master) branch is found at https://projectacrn.github.io/latest/.

ACRN v0.7 requires Clear Linux OS version 28260 or newer.  Please follow the
instructions in the :ref:`getting-started-apl-nuc`.

Version 0.7 new features
************************

Enable cache QOS with CAT
=========================

Cache Allocation Technology (CAT) is enabled on Apollo Lake (APL)
platforms, providing cache isolation between VMs mainly for real-time
performance quality of service (QoS).  The CAT for a specific VM is
normally set up at boot time per the VM configuration determined at
build time. For debugging and performance tuning, the CAT can also be
enabled and configured at runtime by writing proper values to certain
MSRs using the ``wrmsr`` command on ACRN shell.

Support ACPI power key mediator
===============================
ACRN supports ACPI power/sleep key on the APL and KBL NUC platforms,
triggering S3/S5 flow, following the ACPI spec.

Document updates
================
Several new documents have been added in this release, including:

* ACRN Roadmap: look ahead in 2019
* Performance analysis of VBS-k framework
* HLD design doc for IOC virtualization
* Additional project coding guidelines

New Features Details
********************

- :acrn-issue:`940` - Device: IPU support
- :acrn-issue:`1138` - Debug: NPK
- :acrn-issue:`1186` - Disable VBS_DEBUG to improve VBS-K performance
- :acrn-issue:`1508` - DM: customized changes for RPMB mux kernel module
- :acrn-issue:`1536` - dm: add virtio_mei mediator
- :acrn-issue:`1544` - dm: rpmb: Support RPMB mode config from launch.sh
- :acrn-issue:`1812` - export kdf_sha256 interface from crypto lib
- :acrn-issue:`1815` - improve emulation of I/O port CF9
- :acrn-issue:`1915` - Enable Audio Mediator
- :acrn-issue:`1953` - Add cmdline option to disable/enable vhm module for guest
- :acrn-issue:`2176` - Fix RTC issues in ACPI
- :acrn-issue:`2319` - Add vHPET support
- :acrn-issue:`2351` - Enable post-launched hybrid mode
- :acrn-issue:`2407` - Coding style changes for IOAPIC and MSI representation
- :acrn-issue:`2426` - Enable Interrupt Remapping feature
- :acrn-issue:`2431` - VPCI code cleanup
- :acrn-issue:`2448` - Adding support for socket as a backend for Virtio-Console
- :acrn-issue:`2462` - Enable cache QOS with CAT
- :acrn-issue:`2496` - VTD/IOMMU Modulization
- :acrn-issue:`2553` - Disable vTPM feature on ACRN GP2.0(apl_sdc_stable)
- :acrn-issue:`2595` - Update vsbl to v1.2

Fixed Issues Details
********************

- :acrn-issue:`1319` - SD card pass-through: UOS can't see SD card after UOS reboot.
- :acrn-issue:`1774` - UOS cannot stop by command: acrnctl stop [vm name] in SOS
- :acrn-issue:`1780` - Some video formats cannot be played in SOS.
- :acrn-issue:`1782` - UOS failed to get ip with the pass-throughed network card
- :acrn-issue:`1999` - [APLNUC][KBLNUC][APLUP2]UOS reset fails with acrnctl reset command
- :acrn-issue:`2276` - OVMF failed to launch UOS on UP2
- :acrn-issue:`2298` - Hardcodes path to iasl
- :acrn-issue:`2316` - Tools don't respect CFLAGS/LDFLAGS from environment
- :acrn-issue:`2338` - [UP2]Lost 2G memory in SOS when using SBL as bootloader on UP2
- :acrn-issue:`2370` - Doesn't use parallel make in subbuilds
- :acrn-issue:`2422` - [PATCH] profiling: fix the system freeze issue when running profiling  tool
- :acrn-issue:`2453` - Fix vHPET memory leak on device reset
- :acrn-issue:`2455` - host call stack disappear when dumping
- :acrn-issue:`2516` - [UP2][SBL] System hang with DP monitor connected
- :acrn-issue:`2528` - [APLUP2] SBL (built by SBL latest code) failed to boot ACRN hypervisor
- :acrn-issue:`2543` - vLAPIC: DCR not properly initialized
- :acrn-issue:`2548` - [APLNUC/KBLNUC][GVT][SOS/LAAG] Weston fails to play video in SOS and UOS
- :acrn-issue:`2572` - Startup SOS Fails
- :acrn-issue:`2588` - Uninitialized Variable is used in acrn_kernel/drivers/acrn/acrn_trace.c and acrn_hvlog.c
- :acrn-issue:`2606` - HV crash during running VMM related Hypercall fuzzing test.
- :acrn-issue:`2624` - Loading PCI devices with table_count > CONFIG_MAX_MSIX_TABLE_NUM leads to writing outside of struct.
- :acrn-issue:`2643` - Ethernet pass-through, network card can't get ip in uos
- :acrn-issue:`2674` - VGPU needs the lock when updating ppggt/ggtt to avoid the race condition
- :acrn-issue:`2695` - UOS powers off or suspend while pressing power key, UOS has no response

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

:acrn-issue:`1996` - There is an error log when using ``acrnd&`` to boot UOS
   An error log is printed when starting ``acrnd`` as a background job
   (``acrnd&``) to boot UOS. The UOS still boots up
   normally, but prints::

     Failed to open the socket(sos-lcs) to query the reason for the wake-up.
     Activating all vms when acrnd & to boot uos.

   **Impact:** UOS boots normally, but prints an error log message.

   **Workaround:** None.

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

:acrn-issue:`2279` - [APLNUC] After exiting UOS with mediator
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

:acrn-issue:`2522` - [NUC7i7BNH] After starting IAS in SOS, there is no display
   On NUC7i7BNH, after starting IAS in SOS, there is no display if the monitor is
   connected with a TPC-to-VGA connector.

   **Impact:** Special model [NUC7i7BNH] has no display in SOS.

   **Workaround:** None.

-----

:acrn-issue:`2523` - UOS monitor does not display when using IAS
   There is no UOS display after starting IAS weston.

   **Impact:** Cannot use IAS weston in UOS.

   **Workaround:**

   1) Use weston instead of IAS weston: ``swupd install x11-server``
   2) Use acrn-kernel to rebuild SOS kernel to replace integrated kernel.
      Confirm "DRM_FBDEV_EMULATION" related configs in kernel_config_sos are:

      .. code-block:: bash

         CONFIG_DRM_KMS_FB_HELPER=y
         CONFIG_DRM_FBDEV_EMULATION=y
         CONFIG_DRM_FBDEV_OVERALLOC=100

   The issue will be fixed in the next release.

-----

:acrn-issue:`2524` - [UP2][SBL] Launching UOS hangs while weston is running in SOS
   When using weston in SOS, it will hang during the UOS launch.

   **Impact:** launching UOS hangs, and then no display in UOS.

   **Workaround:** Use acrn-kernel to rebuild SOS kernel to replace the
   integrated kernel. Confirm "DRM_FBDEV_EMULATION" related
   configs in kernel_config_sos are:

   .. code-block:: bash

      CONFIG_DRM_KMS_FB_HELPER=y
      CONFIG_DRM_FBDEV_EMULATION=y
      CONFIG_DRM_FBDEV_OVERALLOC=100

   The issue will be fixed in the next release.

-----

:acrn-issue:`2527` - [KBLNUC][HV]System will crash when run ``crashme`` (SOS/UOS)
   System will crash after a few minutes running stress test ``crashme`` tool in SOS/UOS.

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

   git log --pretty=format:'- :acrn-commit:`%h` %s' --after="2018-03-01"

- :acrn-commit:`c72e2e8c` - doc: use the new board name for UP2 in create-up2-images.sh script
- :acrn-commit:`56afe97e` - doc: fix broken external links
- :acrn-commit:`e263b554` - HV: Fix modularization vm config code lost CAT code
- :acrn-commit:`703b366c` - dm: use power button acpi device to find its input event
- :acrn-commit:`8a324060` - ACRN: dm: Fix luanch UOS script "-d" parameter fail issue
- :acrn-commit:`06118998` - OVMF release v0.7
- :acrn-commit:`6794660e` - HV: use the common functions defined in vdev.c to reduce duplicate code
- :acrn-commit:`be3fbaa4` - HV: add generic vdev functions to vdev.c
- :acrn-commit:`731b0444` - HV: rename core.c to vdev.c
- :acrn-commit:`819bcec6` - HV: remove sharing_mode_vdev_array from sharing_mode.c
- :acrn-commit:`00f9b850` - HV: move pci_vdevs[] array from vm.h to vpci.h
- :acrn-commit:`8c3cfe62` - doc: add VBSK overhead analysis doc
- :acrn-commit:`30159d5b` - doc: add some rules related to coding style
- :acrn-commit:`ff65a103` - HV: vm_configs array refinement
- :acrn-commit:`0d90515b` - HV: refine is_lapic_pt
- :acrn-commit:`1bb15c64` - HV: modularization vm config code
- :acrn-commit:`35dfadc9` - dm: check SCI_EN bit of pm1_control before trigger SCI
- :acrn-commit:`566e8824` - dm: power button emulation by acrnctl command.
- :acrn-commit:`32a7b4f1` - doc: add IOC virtualization HLD
- :acrn-commit:`c69dab0a` - hv: add support of EPT mapping of high MMIO
- :acrn-commit:`29b1ebcd` - dm: add support of high MMIO mapping
- :acrn-commit:`32925c10` - dm: allocate 64bit MMIO above 4G strictly to pass OVMF check
- :acrn-commit:`aed75145` - dm: Limit 64 bits PCI BAR region address space
- :acrn-commit:`7628e790` - DM: virtio-gpio: use virtio_base as the first member of virtio_gpio
- :acrn-commit:`a89c41dd` - HV: cleanup header files under hypervisor/common
- :acrn-commit:`3cb5542b` - HV: cleanup header files under hypervisor/dm
- :acrn-commit:`e2995538` - tools: acrn-crashlog: new file to count all events happened in system
- :acrn-commit:`73e53232` - tools: acrn-crashlog: stop only collecting logs when exceeding configured size
- :acrn-commit:`e38ff18b` - hv:cleanup header files for release folder
- :acrn-commit:`33ecdd73` - Makefile: undefine _FORTIFY_SOURCE prior using it
- :acrn-commit:`3b2784ec` - HV: CAT: support config CAT from acrn_vm_config
- :acrn-commit:`43ee5590` - HV: CAT: capability enumaration
- :acrn-commit:`cf524e68` - HV: CAT: add platform specified info for CLOS
- :acrn-commit:`ae34fdd8` - doc: fix misspellings
- :acrn-commit:`a9482f46` - tweak GSG
- :acrn-commit:`2e60adef` - hv: vmcs: simplify update EOI-exit bitmap
- :acrn-commit:`501b3f7e` - hv:cleanup header files for debug folder
- :acrn-commit:`511d4c15` - hv:cleanup console.h
- :acrn-commit:`cca87579` - hv: remove the duplicated init_vm_boot_info() for partition mode
- :acrn-commit:`cf1515d6` - hv: optimize the assignment of load addresses for multiboot images
- :acrn-commit:`3f0ff2ec` - hv: search additional argument when parsing seed from ABL
- :acrn-commit:`f5504e80` - HV: vpci_vdev_array cleanup
- :acrn-commit:`a25f1a40` - HV: remove default folder in configs
- :acrn-commit:`f9b5e21b` - HV: rename board name of up2 to apl-up2
- :acrn-commit:`94e12275` - hv: code style fix for partition mode specific code
- :acrn-commit:`8478a328` - HV: return an error code when REQ state mismatch in acrn_insert_request
- :acrn-commit:`68652104` - ACRN: dm: Modify runC default rootfs directory
- :acrn-commit:`55cb7770` - ACRN: dm: Add new capabilities for runC container
- :acrn-commit:`5690b762` - ACRN: dm: Change runC container's start arguments
- :acrn-commit:`6e919d2a` - ACRN: dm: Add launch container method in script
- :acrn-commit:`f95da183` - dm: acrn-tool: Add del runC configuration in acrnctl del
- :acrn-commit:`a0efd3e5` - dm: acrn-tool: Add new parameter for acrnctl add
- :acrn-commit:`2f7ed65f` - DM: Attestation Keybox support in SOS DM
- :acrn-commit:`987ddafa` - hv: vlapic: refine apicv_post_intr to internal function
- :acrn-commit:`5dd6e79f` - hv: vlapic: refine vlapic_enabled to internal function
- :acrn-commit:`e218efd5` - hv: vm: move vm_active_cpus to vm.h
- :acrn-commit:`780f520f` - DM: virtio-gpio: return a valid length for GPIO request
- :acrn-commit:`8bc0e128` - HV: remove pbdf from struct pci_vdev
- :acrn-commit:`4d119853` - HV: define function bdf_is_equal() to compare bdf
- :acrn-commit:`02866353` - HV: fix comments issue
- :acrn-commit:`1454dd37` - HV: this patch fixes bar address non-zero checking for 64-bit bars
- :acrn-commit:`b43f5cba` - tools: do not include unnecessary files in release build
- :acrn-commit:`eee7d8e7` - hv: debug: mark the mmio address for npk log as hv owned
- :acrn-commit:`bd1e7a46` - hv:cleanup header files for arch folder
- :acrn-commit:`ac7a8a72` - hv:merge MACROs E820_MAX_ENTRIES and NUM_E820_ENTRIES
- :acrn-commit:`fb92d55b` - doc: fix formatting of up2 doc
- :acrn-commit:`1d783d3d` - doc: add 0.6 to doc version menu
- :acrn-commit:`4928be5f` - doc: update partition mode config on up2
- :acrn-commit:`02ae775b` - hv: pae: fix a issue of loading pdptrs when handle cr4
- :acrn-commit:`25385241` - hv: pae: fix bug when calculate PDPT address
- :acrn-commit:`21ae3e74` - DM: virtio-gpio: add print log
- :acrn-commit:`6b0643b5` - DM: virtio-gpio: implementation of gpio opearations
- :acrn-commit:`77e17b5d` - DM: virtio-gpio: gpio initialization.
- :acrn-commit:`57029315` - DM: virtio-gpio: virtio framework implementation.
- :acrn-commit:`5300e911` - config: enable parsing dmar table dynamically on UP2
- :acrn-commit:`8e8ed07d` - dm: implement power button for power managerment
- :acrn-commit:`b24a8a0f` - hv:cleanup header file for guest folder
- :acrn-commit:`75f6cab5` - hv:cleanup header file for per_cpu.h
- :acrn-commit:`c093638b` - hv:merge two header files to one with the same name
- :acrn-commit:`04c30fb3` - hv:move 2 APIs from hypervisor.h to guest_memory.c
- :acrn-commit:`07656a9c` - DM: modify acpi for IASL to support ACPI6.3
- :acrn-commit:`827fffed` - hv: exception: fault type exception should set resume flag in rflags
- :acrn-commit:`26385183` - acrn.conf: clean-up SOS kernel options (EFI platforms)
- :acrn-commit:`caab595e` - hv: vlapic: properly initialize DCR
- :acrn-commit:`614b2ea8` - version: 0.7-unstable

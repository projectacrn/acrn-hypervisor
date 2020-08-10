.. _release_notes_1.1:

ACRN v1.1 (Jun 2019)
####################

We are pleased to announce the release of ACRN version 1.1.

ACRN is a flexible, lightweight reference hypervisor, built with
real-time and safety-criticality in mind, optimized to streamline embedded
development through an open source platform. Check out the :ref:`introduction` for more information.
All project ACRN source code is maintained in the https://github.com/projectacrn/acrn-hypervisor
repository and includes folders for the ACRN hypervisor, the ACRN device
model, tools, and documentation. You can either download this source code as
a zip or tar.gz file (see the `ACRN v1.1 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v1.1>`_)
or use Git clone and checkout commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v1.1

The project's online technical documentation is also tagged to correspond
with a specific release: generated v1.1 documents can be found at https://projectacrn.github.io/1.1/.
Documentation for the latest (master) branch is found at https://projectacrn.github.io/latest/.
ACRN v1.1 requires Clear Linux* OS version 29970. Please follow the
instructions in the :ref:`kbl-nuc-sdc`.

Version 1.1 major features
**************************

Hybrid Mode Introduced
======================
In hybrid mode, a Zephyr OS is launched by the hypervisor even before the Service OS is
launched (pre-launched), with dedicated resources to achieve highest level of isolation.
This is designed to meet the needs of a FuSa certifiable safety OS.

Support for new guest Operating Systems
=======================================
* The `Zephyr RTOS <https://zephyrproject.org>`_ can be a pre-launched Safety OS in hybrid mode.
  It can also be a post-launched (launched by Service OS, not the hypervisor) as a guest OS.
* VxWorks as a post-launched RTOS for industrial usages.
* Windows as a post-launched OS

Document updates
================
We have many `reference documents available <https://projectacrn.github.io>`_, including:

* Update: Using PREEMPT_RT-Linux for real-time UOS
* :ref:`Zephyr RTOS as Guest OS <using_zephyr_as_uos>`
* :ref:`Using VxWorks* as User OS <using_vxworks_as_uos>`
* :ref:`How to enable OVS in ACRN <open_vswitch>`
* :ref:`Enable QoS based on runC container <acrn-dm_qos>`
* :ref:`Using partition mode on NUC <using_partition_mode_on_nuc>`

New Features Details
********************

- :acrn-issue:`2538` - Add new hypercall to fetch platform configurations.
- :acrn-issue:`2587` - ACRN HV: allow pre-launched VMs have different memory size and use > 2GB memory
- :acrn-issue:`2621` - refine code in vlapic_trigger_lvt.
- :acrn-issue:`2635` - OVS/vlan 802.1Q support for ACRN ISD
- :acrn-issue:`2902` - Completely remove enable_bar()/disable_bar() functions
- :acrn-issue:`2962` - Enabling WaaG on ACRN
- :acrn-issue:`3069` - Virtualization will support VxWorks7 64-bit as a GuestVM
- :acrn-issue:`3099` - WaaG: Add WaaG support in launch_uos.sh
- :acrn-issue:`3116` - Get max vcpu per vm from HV instead of hardcode
- :acrn-issue:`3158` - Power Management: Guest control
- :acrn-issue:`3179` - Hypervisor CPU SGX
- :acrn-issue:`3214` - Hypervisor Hybrid Mode
- :acrn-issue:`3222` - Virtual boot loader: Required Optimization and Configuration
- :acrn-issue:`3237` - Enable polling mode for virtio devices
- :acrn-issue:`3257` - Audio devices can be passed through to WaaG

Fixed Issues Details
********************

- :acrn-issue:`1262` - acrnctl tool should not only gives RC 0
- :acrn-issue:`1551` - Samples: Kernel bootchart generation using cmdline.
- :acrn-issue:`1844` - Establish a `@return` keyword for functions not returning any value (`void function()`)
- :acrn-issue:`1917` - Documentation: What does ',b' flag do with virtio-blk?
- :acrn-issue:`2378` - Getting started guide for NUC is not working
- :acrn-issue:`2457` - Compilation on Fedora 29 (for UEFI platforms) is broken
- :acrn-issue:`2515` - Display corruption in SOS on KBL
- :acrn-issue:`2518` - Service OS kernel parameters for i915 need a clean-up (EFI platforms)
- :acrn-issue:`2526` - Hypervisor crash when booting UOS with acrnlog running with mem loglevel=6
- :acrn-issue:`2527` - [KBLNUC][HV]System will crash when run crashme (SOS/UOS)
- :acrn-issue:`2584` - acrn-dm segfaults if virtio-gvt enabled but not enabled in kernel
- :acrn-issue:`2737` - Build ACRN failed in Ubuntu 16.04
- :acrn-issue:`2782` - Wifi Firmware initialization failed happened on Reboot
- :acrn-issue:`2794` - Difficult to use make oldconfig
- :acrn-issue:`2806` - hv: minor issue in sbl_init_vm_boot_info()
- :acrn-issue:`2834` - isolate the MSR IA32_MISC_ENABLE between guest and host.
- :acrn-issue:`2848` - Cannot boot SOS kernel outside of ACRN
- :acrn-issue:`2857` - FAQs for ACRN's memory usage need to be updated
- :acrn-issue:`2971` - PCIE ECFG support for AcrnGT
- :acrn-issue:`2976` - [GVT]don't register memory for gvt in acrn-dm
- :acrn-issue:`2984` - HV will crash if  launch two UOS with same UUID
- :acrn-issue:`2991` - Failed to boot normal vm on the pcpu which ever run lapic_pt vm
- :acrn-issue:`3009` - When running new workload on weston, the last workload animation not disappeared and screen flashed badly.
- :acrn-issue:`3028` - virtio gpio line fd not release
- :acrn-issue:`3032` - Dump stack of mem allocation in irq_disabled after using mempool for ACRN VHM
- :acrn-issue:`3050` - FYI: Kconfiglib major version bumped to 11
- :acrn-issue:`3051` - Storage: Support rescan feature for virtio-blk
- :acrn-issue:`3053` - Emulated USB controller Vendor ID and Device ID are swapped
- :acrn-issue:`3054` - USB webcam not working in UOS
- :acrn-issue:`3073` - HV: hotfix for acpi.c compile error
- :acrn-issue:`3081` - add -w option for uos launch sh
- :acrn-issue:`3085` - Can't set tsc frequency through option -f/--frequency of acrnalyze.py
- :acrn-issue:`3096` - Add one workaround to fix build issue with latest gcc.
- :acrn-issue:`3118` - virtio gpio static variable keeps increasing
- :acrn-issue:`3123` - Remove unused functions in hypervisor and device model.
- :acrn-issue:`3127` - refine the passthrough devices with dictionary
- :acrn-issue:`3128` - Undefined CONFIG_REMAIN_1G_PAGES
- :acrn-issue:`3136` - distinguish between LAPIC_PASSTHROUGH configured vs enabled
- :acrn-issue:`3138` - Increase kernel boot args and VM Name length
- :acrn-issue:`3142` - fix cpu family calculation
- :acrn-issue:`3145` - Only certain guests should be granted the privilege to reset host
- :acrn-issue:`3152` - Use virtio-blk instead passthru devices to boot RT
- :acrn-issue:`3160` - There is a logic bug when set iommu page walk coherent
- :acrn-issue:`3181` - [auto][sit][daily]Case "Hypervisor_Launch_RTVM_on_SATA_Storage" sata disk can not passthru
- :acrn-issue:`3182` - run command "echo c > /proc/sysrq-trigger" can't trigger AaaG warm reboot
- :acrn-issue:`3184` - fail to locate ACPI RSDP table on some EFI platforms
- :acrn-issue:`3188` - dm: update uos patch in launch_hard_rt_vm.sh

Known Issues
************

:acrn-issue:`1773` - USB Mediator: Can't find all devices when multiple USB devices connected
   After booting UOS with multiple USB devices plugged in,
   there's a 60% chance that one or more devices are not discovered.

   **Impact:** Cannot use multiple USB devices at same time.

   **Workaround:** Unplug and plug-in the unrecognized device after booting.

-----

:acrn-issue:`1991` - Input not accepted in UART Console for corner case
   Input is useless in UART Console for a corner case, demonstrated with these steps:

   1) Boot to SOS
   2) ssh into the SOS.
   3) use ``./launch_uos.sh`` to boot UOS.
   4) On the host, use ``minicom -D /dev/ttyUSB0``.
   5) Use ``sos_console 0`` to launch SOS.

   **Impact:** Fails to use UART for input.

   **Workaround:** Enter other keys before typing :kbd:`Enter`.

-----

:acrn-issue:`2267` - [APLUP2][LaaG] LaaG can't detect 4k monitor
   After launching UOS on APL UP2 , 4k monitor cannot be detected.

   **Impact:** UOS can't display on a 4k monitor.

   **Workaround:** Use a monitor with less than 4k resolution.

-----

:acrn-issue:`2279` - [APLNUC] After exiting UOS, SOS can't use USB keyboard and mouse
   After exiting UOS with mediator Usb_KeyBoard and Mouse, SOS cannot use the USB keyboard and mouse.

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
   2) Copy the "Zephyr.img", "OVMF.fd" and "launch_zephyr.sh" to NUC.
   3) Execute the launch_zephyr.sh script.

   This is not reproducible with the stock ``launch_zephyr.sh`` script in our repo,
   it only happens when trying to launch an RTVM.

   **Impact:** Launching Zephyr RTOS as a real-time UOS takes too long

   **Workaround:** A different version of Grub is known to work correctly

-----

:acrn-issue:`3268` - dm: add virtio-rnd device to command line
   LaaG's network is unreachable with UOS kernel

   These steps reproduce the issue:

   1) Download Clear Linux OS
   2) Decompress Clear Linux
   3) Replace above ``kvm.img`` with default kernel in UOS
   4) Launch UOS
   5) Try to ping UOS from another host.
   6) UOS network is unreachable.

   **Impact:** LaaG's network is unreachable with UOS kernel

   **Workaround:** Add ``-s 7,virtio-rnd \`` to the launch_uos.sh script

-----

:acrn-issue:`3280` - AcrnGT holding forcewake lock causes high CPU usage in gvt workload thread.
   The i915 forcewake mechanism is to keep the GPU from its low power state, in
   order to access some specific registers. However, in the path of GVT-g scheduler submission,
   there's no need to acquire the i915 forcewake.

   **Impact:** AcrnGT holding forcewake lock cause high cpu usage gvt workload thread

   **Workaround:** None

-----

:acrn-issue:`3279` - AcrnGT causes display flicker in some situations.
   In current scaler ownership assignment logic, there's an issue that when SOS disables a plane,
   it will disable corresponding plane scalers; however, there's no scaler ownership checking there.
   So the scalers owned by UOS may be disabled by SOS by accident.

   **Impact:** AcrnGT causes display flicker in some situations

   **Workaround:** None

-----

Change Log
**********

These commits have been added to the acrn-hypervisor repo since the v1.0
release in May 2019 (click on the CommitID link to see details):

.. comment

   This list is obtained from this git command (update the date to pick up
   changes since the last release):

   git log --pretty=format:'- :acrn-commit:`%h` - %s' --after="2019-05-09"

- :acrn-commit:`c1e23f1a` - hv:Fix MISRA-C violations for static inline
- :acrn-commit:`93b4cf57` - dm: clean up assert in virtio.c
- :acrn-commit:`c265bd55` - dm: clean up assert in virtio_audio.c
- :acrn-commit:`14a93f74` - dm: clean up assert in virtio_input.c
- :acrn-commit:`0a6baaf4` - dm: samples: use stdio as vxworks console by default
- :acrn-commit:`e3ee9cf2` - HV: fix expression is not boolean
- :acrn-commit:`5cbda22d` - dm: virtio_gpio: clean up assert
- :acrn-commit:`1e23c4dc` - dm: ioc: clean up assert
- :acrn-commit:`8740232a` - HV: Allow pause RTVM when its state is VM_CREATED
- :acrn-commit:`db7e7f1c` - dm: platform: clean up assert() for some platform devices
- :acrn-commit:`1b799538` - dm: pcidev: clean up assert() for some pci devices
- :acrn-commit:`2b3dedfb` - dm: pci: clean up assert() in pci core
- :acrn-commit:`f8934df3` - HV: implement wbinvd instruction emulation
- :acrn-commit:`ea699af8` - HV: Add has_rt_vm API
- :acrn-commit:`7018a13c` - HV: Add ept_flush_leaf_page API
- :acrn-commit:`f320130d` - HV: Add walk_ept_table and get_ept_entry APIs
- :acrn-commit:`f81585eb` - HV: Add flush_address_space API.
- :acrn-commit:`6fd397e8` - HV: Add CLFLUSHOPT instruction.
- :acrn-commit:`d0e08712` - dm: virtio-block: clean up assert
- :acrn-commit:`3ef385d6` - dm: ahci: clean up assert
- :acrn-commit:`4145b8af` - dm: block: clean up assert
- :acrn-commit:`13228d91` - dm: refine 'assert' usage in irq.c and wdt_i6300esb.c
- :acrn-commit:`e6eef9b6` - dm: refine 'assert' usage in pm.c and acpi.c
- :acrn-commit:`885d503a` - dm: refine 'assert' in hugetlb.c and mem.c
- :acrn-commit:`65d7d83b` - refine 'assert' usage in vmmapi.c and main.c
- :acrn-commit:`dedf9bef` - dm: refine 'assert' in inout.c and post.c
- :acrn-commit:`a2332b15` - dm: refine 'assert' usage in timer.c and rtc.c
- :acrn-commit:`ec626482` - dm: cleanup 'assert' for guest software loading module
- :acrn-commit:`0e046c7a` - hv: vlapic: clear which access type we support for APIC-Access VM Exit
- :acrn-commit:`f145cd49` - doc: Instruction of enabling ACRN-DM QoS.
- :acrn-commit:`fd9eb2a5` - HV: Fix OVMF hang issue when boot with lapic_pt
- :acrn-commit:`cdc5f120` - dm: virtio-net: clean up assert
- :acrn-commit:`b0015963` - dm: fix some potential memory leaks
- :acrn-commit:`0620980f` - dm: use strnlen to replace strlen
- :acrn-commit:`1e1244c3` - dm: use strncpy to replace strcpy
- :acrn-commit:`0ea788b4` - dm: passthru: remove the use of assert()
- :acrn-commit:`efccdd22` - dm: add virtio-rnd device to command line
- :acrn-commit:`030e7683` - doc: add systemd-networkd-autostart bundle in APL GSG
- :acrn-commit:`86d3065d` - doc: tweak doxygen precondition label
- :acrn-commit:`48877362` - ACRN: DM: Add new options for NUC launch_uos script.
- :acrn-commit:`f7f2a6ee` - hv: Rename tables member of vPCI msix struct pci_msix
- :acrn-commit:`22f24c22` - DM: Samples: Enable VxWorks as hard-rt VM
- :acrn-commit:`9960ff98` - hv: ept: unify EPT API name to verb-object style
- :acrn-commit:`4add4059` - hv:build system initialization to sys_init_mod.a
- :acrn-commit:`5abca947` - hv: build virtual platform hypercall to vp_hcall_mod.a
- :acrn-commit:`02bf362d` - hv:build virtual platform trusty to vp_trusty_mod.a
- :acrn-commit:`e67f0eab` - hv:build virtual platform DM to vp_dm_mod.a
- :acrn-commit:`4d646c02` - hv:build virtual platform base to vp_base_mod.a
- :acrn-commit:`83e2a873` - hv:build hardware layer to hw_mod.a
- :acrn-commit:`76f21e97` - hv: build boot module to boot_mod.a
- :acrn-commit:`9c81f4c3` - hv:build library to lib_mod.a
- :acrn-commit:`8338cd46` - hv: move 3 files to lib & arch folder
- :acrn-commit:`7d44cd5c` - hv: Introduce check_vm_vlapic_state API
- :acrn-commit:`f3627d48` - hv: Add update_vm_vlapic_state API to sync the VM vLAPIC state
- :acrn-commit:`a3fdc7a4` - hv: Add is_xapic_enabled API to check vLAPIC moe
- :acrn-commit:`7cb71a31` - hv: Make is_x2apic_enabled API visible across source code
- :acrn-commit:`1026f175` - hv: Shuffle logic in vlapic_set_apicbase API implementation
- :acrn-commit:`8426db93` - DM: vrpmb: replace assert() with return false
- :acrn-commit:`66943be3` - dm: enable audio passthrough device.
- :acrn-commit:`cf6d6f16` - doc: remove (outdated) primer documents
- :acrn-commit:`ed7f64d7` - DM: add deinit API for loggers
- :acrn-commit:`d05349d7` - DM: use pr_dbg in vrtc instead of printf
- :acrn-commit:`5ab098ea` - DM: add disk-logger configure in launch script
- :acrn-commit:`c04949d9` - DM: add disk-logger to write log into file system
- :acrn-commit:`6fa41eee` - DM: add static for local variables
- :acrn-commit:`5a9627ce` - DM USB: xHCI: refine the emulation of Stop Endpoint Command
- :acrn-commit:`1be719c6` - DM USB: clean-up: change name of function usb_dev_comp_req
- :acrn-commit:`7dbde276` - DM USB: xHCI: use new isoch transfer implementation
- :acrn-commit:`b57f6f92` - DM USB: clean-up: give shorter names to libusb_xfer and req
- :acrn-commit:`adaed5c0` - DM USB: xHCI: add 'chained' field in struct usb_data_xfer_block
- :acrn-commit:`f2e35ab7` - DM USB: save MaxPacketSize value in endpoint descriptor
- :acrn-commit:`296b649a` - ACRN/HV: emulated pcicfg uses the aligned offset to fix the unaligned pci_cfg access
- :acrn-commit:`2321fcdf` - HV:Modularize vpic code to remove usage of acrn_vm
- :acrn-commit:`c91a5488` - doc: improve clarity of build-from-source intro
- :acrn-commit:`32239cf5` - hv: reduce cyclomatic complexity of create_vm()
- :acrn-commit:`771f15cd` - dm: don't present ioapic and pic to RT VM
- :acrn-commit:`ac6c5dce` - HV: Clean vpic and vioapic logic when lapic is pt
- :acrn-commit:`f83ddd39` - HV: introduce relative vm id for hcall api
- :acrn-commit:`3d3de6bd` - HV: specify dispatch hypercall for sos or trusty
- :acrn-commit:`8c70871f` - doc: add an introduction for building hypervisor
- :acrn-commit:`6b723344` - xsave: inject GP when guest tries to write 1 to XCR0 reserved bit
- :acrn-commit:`d145ac65` - doc: fix typo in the "Build ACRN from Source" guide
- :acrn-commit:`8dd471b3` - hv: fix possible null pointer dereference
- :acrn-commit:`509af784` - dm: Solve the problem of repeat mount hugetblfs
- :acrn-commit:`e5a25749` - doc: add multiboot module string parameter
- :acrn-commit:`e63d32ac` - hv: delay enabling SMEP/SMAP until the end of PCPU initialization
- :acrn-commit:`9e91f14b` - hv: correctly grant DRHD register access rights to hypervisor
- :acrn-commit:`c71cf753` - ACRN/HV: Add one new board configuration for ACRN-hypervisor
- :acrn-commit:`115ba0e3` - Added recommended BIOS settings for better real-time performance.
- :acrn-commit:`7c45f3b5` - doc: remove 'reboot' command from ACRN shell user guide
- :acrn-commit:`04d82e5c` - HV: return virtual lapic id in vcpuid 0b leaf
- :acrn-commit:`0a748fed` - HV: add hybrid scenario
- :acrn-commit:`a2c6b116` - HV: change nuc7i7bnh ram start to 0x60000000
- :acrn-commit:`31aa37d3` - HV: remove unused INVALID_VM_ID
- :acrn-commit:`50e09c41` - HV: remove cpu_num from vm configurations
- :acrn-commit:`f4e976ab` - HV: return -1 with invalid vcpuid in pt icr access
- :acrn-commit:`ae7dcf44` - HV: fix wrong log when vlapic process init sipi
- :acrn-commit:`765669ee` - dm: support VMs communication with virtio-console
- :acrn-commit:`c0bffc2f` - dm: virtio: refine console options parse code
- :acrn-commit:`ce6e663f` - OVMF release v1.1
- :acrn-commit:`0baf537a` - HV: misra fix for patch set of Zephyr enabling
- :acrn-commit:`1906def2` - HV: enable load zephyr kernel
- :acrn-commit:`6940cabd` - HV: modify ve820 to enable low mem at 0x100000
- :acrn-commit:`ea7ca859` - HV: use tag to specify multiboot module
- :acrn-commit:`d0fa83b2` - HV: move sos bootargs to vm configurations
- :acrn-commit:`8256ba20` - HV: add board specific config header
- :acrn-commit:`bb55489e` - HV: make vm kernel type configurable
- :acrn-commit:`ae8893cb` - HV: use api to get kernel load addr
- :acrn-commit:`1c006113` - HV: separate linux loader from direct boot sw loader
- :acrn-commit:`0f00a4b0` - HV: refine sw_linux struct
- :acrn-commit:`475b05da` - doc: fix formatting in partition doc
- :acrn-commit:`76346fd2` - doc: setup logical partition scenario on nuc
- :acrn-commit:`6f61aa7d` - doc: add instruction of Open vSwitch
- :acrn-commit:`a6bba6bc` - doc: update prefix format in coding guidelines
- :acrn-commit:`a4e28213` - DM: handle SIGPIPE signal
- :acrn-commit:`19366458` - DM: handle virtio-console writes when no socket backend is connected
- :acrn-commit:`376fcddf` - HV: vuart: add vuart_deinit during vm shutdown
- :acrn-commit:`81cbc636` - HV: vuart: Bugfix for no interrupts on THRE
- :acrn-commit:`857e6c04` - dm: passthrough: allow not page-aligned sized bar to be mapped
- :acrn-commit:`b98096ea` - dm: pci: fix the MMIO regions overlap when getting bar size
- :acrn-commit:`011134d5` - doc: Update Using PREEMPT_RT-Linux for real-time UOS
- :acrn-commit:`5533263e` - tools:acrn-crashlog: fix error logs writing to server
- :acrn-commit:`286dd180` - dm: virtio: bugfix for polling mode
- :acrn-commit:`4c09051c` - hv: Remove unused variable in ptirq_msi_info
- :acrn-commit:`34f12219` - hv: use 64bit FACS table address only beyond ACPI2.0
- :acrn-commit:`811d1fe9` - dm: pci: update MMIO EPT mapping when update BAR address
- :acrn-commit:`cee2f8b2` - ACRN/HV: Refine the function of init_vboot to initialize the depriv_boot env correctly
- :acrn-commit:`1c36508e` - ACRN/HV: Assign the parsed RSDP to acpi_rsdp to avoid multiple RSDP parsing
- :acrn-commit:`c5d43657` - hv: vmcs: don't trap when setting reserved bit in cr0/cr4
- :acrn-commit:`f2c53a98` - hv: vmcs: trap CR4.SMAP/SMEP/PKE setting
- :acrn-commit:`a7389686` - hv: Precondition checks for vcpu_from_vid for lapic passthrough ICR access
- :acrn-commit:`7f648d75` - Doc: Cleanup vmcfg in user guides
- :acrn-commit:`9aa06c6e` - Doc: Cleanup vmcfg in HLD
- :acrn-commit:`50f50872` - DM: Cleanup vmcfg
- :acrn-commit:`7315515c` - DM: Cleanup vmcfg APIs usage for removing the entire vmcfg
- :acrn-commit:`a3073175` - dm: e820: reserve memory range for EPC resource
- :acrn-commit:`7a915dc3` - hv: vmsr: present sgx related msr to guest
- :acrn-commit:`1724996b` - hv: vcpuid: present sgx capabilities to guest
- :acrn-commit:`65d43728` - hv: vm: build ept for sgx epc resource
- :acrn-commit:`c078f90d` - hv: vm_config: add epc info in vm config
- :acrn-commit:`245a7320` - hv: sgx: add basic support to init sgx resource for vm
- :acrn-commit:`c5cfd7c2` - vm state: reset vm state to VM_CREATED when reset_vm is called
- :acrn-commit:`610ad0ce` - dm: update uos path in launch_hard_rt_vm.sh
- :acrn-commit:`b27360fd` - doc: update function naming convention
- :acrn-commit:`b833e2f9` - hv: vtd: fix a logic error when set iommu page walk coherent
- :acrn-commit:`517707de` - DM/HV: Increase VM name len
- :acrn-commit:`f010f99d` - DM: Decouple and increase kernel boot args length
- :acrn-commit:`f2fe3547` - HV: remove mptable in vm_config
- :acrn-commit:`26c7e372` - Doc: Add tutorial about using VxWorks as uos
- :acrn-commit:`b10ad4b3` - DM USB: xHCI: refine the logic of CCS bit of PORTSC register
- :acrn-commit:`ae066689` - DM USB: xHCI: re-implement the emulation of extended capabilities
- :acrn-commit:`5f9cd253` - Revert "DM: Get max vcpu per vm from HV instead of hardcode"
- :acrn-commit:`8bca0b1a` - DM: remove unused function mptable_add_oemtbl
- :acrn-commit:`bd3f34e9` - DM: remove unused function vm_get_device_fd
- :acrn-commit:`9224277b` - DM: remove unused function vm_setup_ptdev_msi
- :acrn-commit:`bb8584dd` - DM: remove unused function vm_apicid2vcpu
- :acrn-commit:`ec924385` - DM: remove unused function vm_create_devmem
- :acrn-commit:`75ef7e84` - DM: remove unused function vm_set_lowmem_limit
- :acrn-commit:`683e2416` - DM: remove unused function console_ptr_event
- :acrn-commit:`12f55d13` - DM: remove unused function console_key_event
- :acrn-commit:`aacc6e59` - DM: remove unused function console_refresh
- :acrn-commit:`2711e553` - DM: remove unused function console_fb_register
- :acrn-commit:`d19d0e26` - DM: remove unused function gc_init
- :acrn-commit:`43c01f8e` - DM: remove unused function console_init
- :acrn-commit:`e6360b9b` - DM: remove unused function gc_resize
- :acrn-commit:`d153bb86` - DM: remove unused function gc_set_fbaddr
- :acrn-commit:`475c51e5` - DM: remove unused function console_set_fbaddr
- :acrn-commit:`4e770316` - DM: remove unused function swtpm_reset_tpm_established_flag
- :acrn-commit:`2a33d52e` - DM: remove unused function vrtc_reset
- :acrn-commit:`1a726ce0` - DM: remove unused function vrtc_get_time
- :acrn-commit:`8cb64cc7` - DM: remove unused function vrtc_nvram_read
- :acrn-commit:`dcd6d8b5` - DM: remove unused function virtio_pci_modern_cfgread and virtio_pci_modern_cfgwrite
- :acrn-commit:`62f14bb1` - DM: remove unused function virtio_dev_error
- :acrn-commit:`2d6e6ca3` - DM: remove unused function usb_native_is_ss_port
- :acrn-commit:`7e80a6ee` - hv: vm: enable iommu after vpci init
- :acrn-commit:`bfc08c28` - hv: move msr_bitmap from acrn_vm to acrn_vcpu_arch
- :acrn-commit:`f96ae3f1` - HV: enforce Cx of apl nuc with SPACE_SYSTEM_IO
- :acrn-commit:`1fe57111` - HV: validate pstate by checking px ctl range
- :acrn-commit:`57275a58` - HV: add px cx data for kbl nuc refresh
- :acrn-commit:`3d459dfd` - DM: Fix minor issue of USB vendor ID
- :acrn-commit:`7e520675` - doc: update coding guidelines
- :acrn-commit:`04ccaacb` - hv:not allow SOS to access prelaunched vm memory
- :acrn-commit:`0a461a45` - tools:acrn-crashlog: fix potential memory corruption
- :acrn-commit:`5a23f7b6` - hv: initial host reset implementation
- :acrn-commit:`321e4f13` - DM: add virtual hostbridge in launch script for RTVM
- :acrn-commit:`d0fe1820` - HV: call function is_prelaunched_vm() instead to reduce code
- :acrn-commit:`a6503c6a` - HV: remove function pci_pdev_foreach()
- :acrn-commit:`536c69b9` - hv: distinguish between LAPIC_PASSTHROUGH configured vs enabled
- :acrn-commit:`cb6a3e8f` - doc: prepare for sphinx 2.0 upgrade
- :acrn-commit:`474496fc` - doc: remove hard-coded interfaces in .rst files
- :acrn-commit:`ffb92454` - doc: add note to indicate that vSBL is only supported on APL platforms
- :acrn-commit:`c561f2d6` - doc: add <vm_id> parameter to the "vm_console" command description
- :acrn-commit:`214eb5e9` - doc: Update clearlinux os installation guide link.
- :acrn-commit:`fe4fcf49` - xHV: remove unused function is_dbg_uart_enabled
- :acrn-commit:`36568ff5` - HV: remove unused function sbuf_is_empty and sbuf_get
- :acrn-commit:`c5391d25` - HV: remove unused function vcpu_inject_ac
- :acrn-commit:`26de86d7` - HV: remove unused function copy_to_gva
- :acrn-commit:`163c63d2` - HV: remove unused function resume_vm
- :acrn-commit:`c68c6e4a` - HV: remove unused function shutdown_vcpu
- :acrn-commit:`83012a5a` - HV: remove unused function disable_iommu
- :acrn-commit:`9b7dee90` - HV: remove one lock for ctx->flags operation.
- :acrn-commit:`fc1cbebe` - HV: remove vcpu arch lock, not needed.
- :acrn-commit:`9876138b` - HV: add spinlock to dmar_enable/disable_qi
- :acrn-commit:`90f3ce44` - HV: remove unused UNDEFINED_VM
- :acrn-commit:`73cff9ef` - HV: predefine pci vbar's base address for pre-launched VMs in vm_config
- :acrn-commit:`4cdaa519` - HV: rename vdev_pt_cfgwrite_bar to vdev_pt_write_vbar and some misra-c fix
- :acrn-commit:`aba357dd` - 1. fix cpu family calculation 2. Modify the parameter 'fl' order
- :acrn-commit:`238d8bba` - reshuffle init_vm_boot_info
- :acrn-commit:`0018da41` - HV: add missing @pre for some functions
- :acrn-commit:`b9578021` - HV: unify the sharing_mode_cfgwrite and partition_mode_cfgwrite code
- :acrn-commit:`7635a68f` - HV: unify the sharing_mode_cfgread and partition_mode_cfgread code
- :acrn-commit:`19af3bc8` - HV: unify the sharing_mode_vpci_deinit and partition_mode_vpci_deinit code
- :acrn-commit:`3a6c63f2` - HV: unify the sharing_mode_vpci_init and partition_mode_vpci_init code
- :acrn-commit:`f873b843` - HV: cosmetic fix for pci_pt.c
- :acrn-commit:`cf48b9c3` - HV: use is_prelaunched_vm/is_hostbridge to check if the code is only for pre-launched VMs
- :acrn-commit:`a97e6e64` - HV: rename sharing_mode_find_vdev_sos to find_vdev_for_sos
- :acrn-commit:`32d1a9da` - HV: move bar emulation initialization code to pci_pt.c
- :acrn-commit:`67b2e2b8` - HV: Remove undefined CONFIG_REMAIN_1G_PAGES
- :acrn-commit:`bb48a66b` - dm: refine the passthrough devices with dictionary
- :acrn-commit:`517cff1b` - hv:remove some unnecessary includes
- :acrn-commit:`49350634` - DM: virtio-gpio: fixed static variable keeps increasing issue
- :acrn-commit:`865ee295` - hv: emulate ACPI reset register for Service OS guest
- :acrn-commit:`26f08680` - hv: shutdown guest VM upon triple fault exceptions
- :acrn-commit:`9aa3fe64` - hv: emulate reset register 0xcf9 and 0x64
- :acrn-commit:`8ad0fd98` - hv: implement NEED_SHUTDOWN_VM request to idle thread
- :acrn-commit:`db952315` - HV: fix MISRA violation of host_pm.h
- :acrn-commit:`1aac0dff` - HV: hot fix on usage of CONFIG_ACPI_PARSE_ENABLED
- :acrn-commit:`356bf184` - DM: Get max vcpu per vm from HV instead of hardcode
- :acrn-commit:`86f5993b` - hv: vlapic: fix tpr virtualization when VID is not enabled.
- :acrn-commit:`a68dadb7` - hv: vm: minor fix about vRTC
- :acrn-commit:`8afbdb75` - HV: enable Kconfig of ACPI_PARSE_ENABLED
- :acrn-commit:`86fe2e03` - HV: split acpi.c
- :acrn-commit:`cbab1f83` - HV: remove acpi_priv.h
- :acrn-commit:`565f3cb7` - HV: move dmar parse code to acpi parser folder
- :acrn-commit:`39798691` - hv:merge static_checks.c
- :acrn-commit:`d9717967` - dm:add grep -w option for uos launch sh
- :acrn-commit:`4c28a374` - dm: add sample script to launch Windows as guest
- :acrn-commit:`bdae8efb` - hv: instr_emul: fix movzx return memory opsize wrong
- :acrn-commit:`795d6de0` - hv:move several files related X86 for lib
- :acrn-commit:`350d6a9e` - hv:Move BUS_LOCK to atomic.h
- :acrn-commit:`eff44fb0` - build fix: fix build issue with latest gcc for blkrescan
- :acrn-commit:`c7da3976` - Dockerfile: update Ubuntu 16.04 Dockerfile to include all deps
- :acrn-commit:`7b8abe15` - hv: refine 'init_percpu_lapic_id'
- :acrn-commit:`dbb41575` - hv: remove dynamic memory allocation APIs
- :acrn-commit:`773889bb` - hv: dmar_parse: remove dynamic memory allocation
- :acrn-commit:`5629ade9` - hv: add default DRHD MACROs in template platform acpi info
- :acrn-commit:`5d192288` - Doc: virtio-blk: add description of boot device option
- :acrn-commit:`d9e6cdb5` - dm: not register/unregister gvt bar memory
- :acrn-commit:`a581f506` - hv: vmsr: enable msr ia32_misc_enable emulation
- :acrn-commit:`8e310e6e` - hv: vcpuid: modify vcpuid according to msr ia32_misc_enable
- :acrn-commit:`ef19ed89` - hv: vcpuid: reduce the cyclomatic complexity of function guest_cpuid
- :acrn-commit:`f0d06165` - hv: vmsr: handle guest msr ia32_misc_enable read/write
- :acrn-commit:`a0a6eb43` - hv: msr: use UL since ia32_misc_enable is 64bit
- :acrn-commit:`7494ed27` - Clean up MISRA C violation
- :acrn-commit:`d364d352` - reshuffle struct vboot_candidates
- :acrn-commit:`41ac9e5d` - rename function & definition from firmware to guest boot
- :acrn-commit:`20f97f75` - restruct boot and bsp dir for firmware stuff

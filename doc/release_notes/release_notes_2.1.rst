.. _release_notes_2.1:

ACRN v2.1 (Aug 2020)
####################

We are pleased to announce the release of the Project ACRN
hypervisor version 2.1.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline
embedded development through an open source platform. Check out
:ref:`introduction` introduction for more information.  All project ACRN
source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, tools, and
documentation. You can either download this source code as a zip or
tar.gz file (see the `ACRN v2.1 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v2.1>`_) or
use Git clone and checkout commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v2.1

The project's online technical documentation is also tagged to
correspond with a specific release: generated v2.1 documents can be
found at https://projectacrn.github.io/2.1/.  Documentation for the
latest under-development branch is found at
https://projectacrn.github.io/latest/.

ACRN v2.1 requires Ubuntu 18.04.  Follow the instructions in the
:ref:`rt_industry_ubuntu_setup` to get started with ACRN.

We recommend that all developers upgrade to ACRN release v2.1.

What's new in v2.1
******************

* Preempt-RT Linux has been validated as a pre-launched realtime VM. See
  :ref:`pre_launched_rt` for more details.

* A Trusted Platform Module (TPM) MMIO device can be passthroughed to a
  pre-launched VM (with some limitations discussed in
  :ref:`mmio-device-passthrough`).  Previously passthrough was only
  supported for PCI devices.

* Open Virtual Machine Firmware (OVMF) now uses a Local Advanced
  Programmable Interrupt Controller (LAPIC) timer as its local time
  instead of the High Precision Event Timer (HPET). This provides the
  working timer service for the realtime virtual machine (RTVM) booting
  process.

* Grub is the recommended bootloader for ACRN.  For more information,
  see :ref:`using_grub`.

Improvements, updates, and corrections have been made throughout our documentation,
including these:

* :ref:`contribute_guidelines`
* :ref:`hv_rdt`
* :ref:`ivshmem-hld`
* :ref:`mmio-device-passthrough`
* :ref:`virtio-net`
* :ref:`getting-started-building`
* :ref:`acrn_configuration_tool`
* :ref:`pre_launched_rt`
* :ref:`rdt_configuration`
* :ref:`using_hybrid_mode_on_nuc`
* :ref:`using_partition_mode_on_nuc`
* :ref:`using_windows_as_uos`
* :ref:`debian_packaging`

Fixed Issues Details
********************
- :acrn-issue:`4047` -  [WHL][Function][WaaG] passthru usb, Windows will hang when reboot it
- :acrn-issue:`4691` -  [WHL][Function][RTVM]without any virtio device, with only pass-through devices, RTVM can't boot from SATA
- :acrn-issue:`4711` -  [WHL][Stabilty][WaaG]Failed to boot up WaaG with core dumped in WaaG reboot test in GVT-d & CPU sharing env.
- :acrn-issue:`4897` -  [WHL][Yocto][GVT-d]WaaG reboot failed due to USB mediator trouble in WaaG reboot stability test.
- :acrn-issue:`4937` -  [EHL][Yocto] Fail to boot ACRN on Yocto
- :acrn-issue:`4958` -  cleanup spin lock in hypervisor
- :acrn-issue:`4989` -  [WHL][Yocto][acrn-configuration-tool] Fail to generate board xml on Yocto build
- :acrn-issue:`4991` -  [WHL][acrn-configuration-tool] vuart1 of VM1 does not change correctly
- :acrn-issue:`4994` -  Default max MSIx table is too small
- :acrn-issue:`5013` -  [TGL][Yocto][YaaG] Can't enter console #1 via HV console
- :acrn-issue:`5015` -  [EHL][TGL][acrn-configuration-tool] default industry xml is only support 2 user vms
- :acrn-issue:`5016` -  [EHL][acrn-configuration-tool] Need update pci devices for ehl industry launch xmls
- :acrn-issue:`5029` -  [TGL][Yocto][GVT] can not boot and login waag with GVT-D
- :acrn-issue:`5039` -  [acrn-configuration-tool]minor fix for launch config tool
- :acrn-issue:`5041` -  Pre-Launched VM boot not successful if SR-IOV PF is passed to
- :acrn-issue:`5049` -  [WHL][Yocto][YaaG] Display stay on openembedded screen when launch YaaG with GVT-G
- :acrn-issue:`5056` -  [EHL][Yocto]Can't enable SRIOV on EHL SOS
- :acrn-issue:`5062` -  [EHL] WaaG cannot boot on EHL when CPU sharing is enabled
- :acrn-issue:`5066` -  [WHL][Function] Fail to launch YaaG with usb mediator enabled
- :acrn-issue:`5067` -  [WHL][Function][WaaG] passthru usb, Windows will hang when reboot it
- :acrn-issue:`5085` -  [EHL][Function]Can't enable SRIOV  when add memmap=64M$0xc0000000 in cmdline on EHL SOS
- :acrn-issue:`5091` -  [TGL][acrn-configuration-tool] generate tgl launch script fail
- :acrn-issue:`5092` -  [EHL][acrn-config-tool]After WebUI Enable CDP_ENABLED=y ,build hypervisor fail
- :acrn-issue:`5094` -  [TGL][acrn-configuration-tool] Board xml does not contain SATA information
- :acrn-issue:`5095` -  [TGL][acrn-configuration-tool] Missing some default launch script xmls
- :acrn-issue:`5107` -  Fix size issue used for memset in create_vm
- :acrn-issue:`5115` -  [REG][WHL][WAAG] Shutdown waag fails under CPU sharing status
- :acrn-issue:`5122` -  [WHL][Stabilty][WaaG][GVT-g & GVT-d]Failed to boot up SOS in cold boot test.

Known Issues
************
- :acrn-issue:`4313` - [WHL][VxWorks] Failed to ping when VxWorks passthru network
- :acrn-issue:`5150` - [REG][WHL][[Yocto][Passthru] Launch RTVM fails with usb passthru
- :acrn-issue:`5151` - [WHL][VxWorks] Launch VxWorks fails due to no suitable video mode found
- :acrn-issue:`5152` - [WHL][Yocto][Hybrid] in hybrid mode ACRN HV env, can not shutdown pre-lanuched RTVM
- :acrn-issue:`5154` - [TGL][Yocto][PM] 148213_PM_SystemS5 with life_mngr fail
- :acrn-issue:`5157` - [build from source] during build HV with XML, “TARGET_DIR=xxx” does not work

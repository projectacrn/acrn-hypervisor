.. _release_notes_2.6:

ACRN v2.6 (Sep 2021) - Draft
############################

We are pleased to announce the release of the Project ACRN hypervisor
version 2.6.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline
embedded development through an open-source platform. See the
:ref:`introduction` introduction for more information.

All project ACRN source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, tools, and
documentation. You can download this source code either as a zip or
tar.gz file (see the `ACRN v2.6 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v2.6>`_) or
use Git ``clone`` and ``checkout`` commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v2.6

The project's online technical documentation is also tagged to
correspond with a specific release: generated v2.6 documents can be
found at https://projectacrn.github.io/2.6/.  Documentation for the
latest development branch is found at https://projectacrn.github.io/latest/.

ACRN v2.6 requires Ubuntu 18.04.  Follow the instructions in the
:ref:`gsg` to get started with ACRN.


What's New in v2.6
******************

Topic
  Description


Upgrading to v2.6 From Previous Releases
****************************************

We highly recommended that you follow these instructions to
upgrade to v2.6 from previous ACRN releases.


Document Updates
****************

We've made major improvements to the introductory ACRN documentation including:

* :ref:`introduction`
* :ref:`overview_dev`
* :ref:`gsg`
* :ref:`acrn_configuration_tool`

We’ve also made edits throughout the documentation to improve clarity,
formatting, and presentation:

.. rst-class:: rst-columns2

* :ref:`hld-devicemodel`
* :ref:`hld-overview`
* :ref:`hld-power-management`
* :ref:`hld-virtio-devices`
* :ref:`hld-io-emulation`
* :ref:`virtio-net`
* :ref:`acrn_on_qemu`
* :ref:`cpu_sharing`
* :ref:`enable-ptm`
* :ref:`nested_virt`
* :ref:`setup_openstack_libvirt`
* :ref:`using_hybrid_mode_on_nuc`
* :ref:`acrn_doc`

Fixed Issues Details
********************

.. comment example item
   - :acrn-issue:`5626` - [CFL][industry] Host Call Trace once detected

- :acrn-issue:`6012` -  [Mainline][PTCM] [ConfigTool]Obsolete terms cleanup for SSRAM
- :acrn-issue:`6284` -  [v2.6] vulnerable coding style in hypervisor and DM
- :acrn-issue:`6340` -  [EF]Invalid LPC entry prevents GOP driver from working properly in WaaG for DP3
- :acrn-issue:`6342` -  [v2.6] vulnerable coding style in config tool python source
- :acrn-issue:`6360` -  ACRN Makefile missing dependencies
- :acrn-issue:`6366` -  TPM pass-thru shall be able to support start method 6, not only support Start Method of 7
- :acrn-issue:`6388` -  [hypercube][tgl][ADL]AddressSanitizer: SEGV virtio_console
- :acrn-issue:`6389` -  [hv ivshmem] map SHM BAR with PAT ignored
- :acrn-issue:`6405` -  [ADL-S][Industry][Yocto] WaaG BSOD in startup when run reboot or create/destory stability test.
- :acrn-issue:`6417` -  ACRN ConfigTool improvement from DX view
- :acrn-issue:`6423` -  ACPI NVS region might not be mapped on prelaunched-VM
- :acrn-issue:`6428` -  [acrn-configuration-tool] Fail to generate launch script when disable CPU sharing
- :acrn-issue:`6431` -  virtio_console use-after-free
- :acrn-issue:`6434` -  HV panic when SOS VM boot 5.4 kernel
- :acrn-issue:`6442` -  [EF]Post-launched VMs do not boot with "EFI Network" enabled
- :acrn-issue:`6461` -  [config_tools] kernel load addr/entry addr should not be configurable for kernel type KERNEL_ELF
- :acrn-issue:`6473` -  [HV]HV can't be used after dumpreg rtvm vcpu
- :acrn-issue:`6476` -  [hypercube][TGL][ADL]pci_xhci_insert_event SEGV on read from NULL
- :acrn-issue:`6481` -  ACRN on QEMU can't boot up with v2.6 branch
- :acrn-issue:`6482` -  [ADL-S][RTVM]rtvm poweroff causes sos to crash
- :acrn-issue:`6502` -  [ADL][HV][UC lock] SoS kernel panic when #GP for UC lock enabled
- :acrn-issue:`6507` -  [TGL][HV][hybrid] during boot zephyr64.elf find HV error: "Unable to copy HPA 0x100000 to GPA 0x7fe00000 in VM0"
- :acrn-issue:`6508` -  [HV]Refine pass-thru device PIO BAR handling
- :acrn-issue:`6510` -  [ICX-RVP][SSRAM] No SSRAM entries  in guest PTCT
- :acrn-issue:`6518` -  [hypercube][ADL]acrn-dm program crash during hypercube testing
- :acrn-issue:`6528` -  [TGL][HV][hybrid_rt] dmidecode Fail on pre-launched RTVM
- :acrn-issue:`6530` -  [ADL-S][EHL][Hybrid]Path of sos rootfs in hybrid.xml is wrong
- :acrn-issue:`6533` -  [hypercube][tgl][ADL] mem leak while poweroff in guest
- :acrn-issue:`6592` -  [doc] failed to make hvdiffconfig

Known Issues
************

- :acrn-issue:`6630` -  Fail to enable 7 PCI based VUART on 5.10.56 RTVM
- :acrn-issue:`6631` -  [KATA][5.10 Kernel]failed to start docker with ServiceVM 5.10 kernel


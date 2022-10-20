.. _release_notes_3.0:

ACRN v3.0 (Jun 2022)
####################

We are pleased to announce the release of the Project ACRN hypervisor
version 3.0.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline
embedded development through an open-source platform. See the
:ref:`introduction` introduction for more information.

All project ACRN source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, tools, and
documentation. You can download this source code either as a zip or
tar.gz file (see the `ACRN v3.0 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v3.0>`_) or
use Git ``clone`` and ``checkout`` commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v3.0

The project's online technical documentation is also tagged to
correspond with a specific release: generated v3.0 documents can be
found at https://projectacrn.github.io/3.0/.  Documentation for the
latest development branch is found at https://projectacrn.github.io/latest/.

ACRN v3.0 requires Ubuntu 20.04.  Follow the instructions in the
:ref:`gsg` to get started with ACRN.


What's New in v3.0
******************

Redesigned ACRN Configuration
  We heard your feedback: ACRN configuration is difficult, confusing, and had
  too many parameters that were not easy to understand.  Release v3.0 features a
  new ACRN Configurator UI tool with a more intuitive design and workflow that
  simplifies getting the setup for the ACRN hypervisor right.  You'll also see
  changes for configuring individual VMs.  We've greatly reduced the number of
  parameters needing your attention,  organized them into basic and advanced
  categories, provided practical defaults, and added error checking so you can
  be much more confident in your configuration before building ACRN.  We've also
  integrated the previously separated scenario and launch options into a merged
  scenario XML configuration file managed by the new Configurator. Read more
  in the :ref:`acrn_configurator_tool` page.

  This is our first major step of continued ACRN user experience improvements.
  If you have feedback on this, or other aspects of ACRN, please share them on
  the `ACRN users mailing list <https://lists.projectacrn.org/g/acrn-users>`_.

  We've also simplified installation of the Configurator by providing a Debian
  package that you can download from the `ACRN v3.0 tag assets
  <https://github.com/projectacrn/acrn-hypervisor/releases/download/v3.0/acrn-configurator-3.0.deb>`_
  and install.  See the :ref:`gsg` for more information.

Improved Board Inspector Collection and Reporting
  You run the ACRN Board Inspector tool to collect information about your target
  system's processors, memory, devices, and more. The generated board XML file
  is used by the ACRN Configurator to determine which ACRN configuration options
  are possible, as well as possible values for target system resources. The v3.0
  Board Inspector has improved scanning and provides more messages about
  potential issues or limitations of your target system that could impact ACRN
  configuration options.  Read more in :ref:`board_inspector_tool`.

.. _Vecow SPC-7100:
   https://marketplace.intel.com/s/offering/a5b3b000000PReMAAW/vecow-spc7100-series-11th-gen-intel-core-i7i5i3-processor-ultracompact-f


Commercial off-the-shelf Tiger Lake machine support
  The `Vecow SPC-7100`_ system is validated and supported by ACRN. This is a
  commercially available 11th Generation Intel® Core™ Processor (codenamed Tiger
  Lake) from Vecow. Read more in the :ref:`hardware` documentation.

Refined shutdown & reset sequence
  A Windows User VM can now shut down or reset the system gracefully. This
  supports a user model where a Windows-based VM provides a system management
  interface. This shutdown capability is achieved by lifecycle managers in each
  VM that talk to each other via a virtual UART channel.

Hypervisor Real Time Clock (RTC)
  Each VM now has its own PC/AT-compatible RTC/CMOS device emulated by the
  hypervisor.  With this, we can avoid any sudden jump in a VM's system clock
  that may confuse certain applications.

ACRN Debianization
  We appreciate a big contribution from the ACRN community! Helmut Buchsbaum from
  TTTech Industrial submitted a "debianization" feature that lets developers
  build and package ACRN into several Debian packages, install them on the target Ubuntu
  or Debian OS, and reboot the machine with ACRN running. Read more in
  :acrn_file:`debian/README.rst`.


Upgrading to v3.0 from Previous Releases
****************************************

With the introduction of the Configurator UI tool, the need for manually editing
XML files is gone.  While working on this improved Configurator, we've also made
many adjustments to available options in the underlying XML files, including
merging the previous scenario and launch XML files into a combined scenario XML
file.  The board XML file generated by the v3.0 Board Inspector tool includes
more information about the target system that is needed by the v3.0
Configurator.

We recommend you generate a new board XML for your target system with the v3.0
Board Inspector.  You should also use the v3.0 Configurator to generate a new
scenario XML file and launch scripts. Scenario XML files and launch scripts
created by previous ACRN versions will not work with the v3.0 ACRN hypervisor
build process and could produce unexpected errors during the build.

Given the scope of changes for the v3.0 release, we have recommendations for how
to upgrade from prior ACRN versions:

1. Start fresh from our :ref:`gsg`. This is the best way to ensure you have a
   v3.0-ready board XML file from your target system and generate a new scenario
   XML and launch scripts from the new ACRN Configurator that are consistent and
   will work for the v3.0 build system.
#. Use the :ref:`upgrade tool <upgrading_configuration>` to attempt
   upgrading configuration files that worked with a release before v3.0.  You’ll
   need the matched pair of scenario XML and launch XML files from a prior
   configuration, and use them to create a new merged scenario XML file.  See
   :ref:`upgrading_configuration` for details.
#. Manually edit your prior scenario XML and launch XML files to make them
   compatible with v3.0.  This is not our recommended approach.

Here are some additional details about upgrading to the v3.0 release.

Generate New Board XML
======================

Board XML files, generated by ACRN board inspector, contain board information
that is essential for building the ACRN hypervisor and setting up User VMs.
Compared to previous versions, ACRN v3.0 adds the following information to the board
XML file for supporting new features and fixes:

  - Add ``--add-llc-cat`` to Board Inspector command line options to manually
    provide Cache Allocation Technology (CAT) to the generated board XML when
    the target hardware does not report availability of this feature.  See
    :ref:`Board Inspector Command-Line Options <board_inspector_cl>` and PR `#7331
    <https://github.com/projectacrn/acrn-hypervisor/pull/7331>`_.
  - Collect all information about SR-IOV devices: see PR `#7302 <https://github.com/projectacrn/acrn-hypervisor/pull/7302>`_.
  - Extract all serial TTYs and virtio input devices: see PR `#7219 <https://github.com/projectacrn/acrn-hypervisor/pull/7219>`_.
  - Extract common ioapic information such as ioapic id, address, gsi base, and gsi num:
    see PR `#6987 <https://github.com/projectacrn/acrn-hypervisor/pull/6987>`_.
  - Add another level of ``die`` node even though the hardware reports die topology in CPUID:
    see PR `#7080 <https://github.com/projectacrn/acrn-hypervisor/pull/7080>`_.
  - Bring up all cores online so Board Inspector can run cpuid to extract all available cores'
    information: see PR `#7120 <https://github.com/projectacrn/acrn-hypervisor/pull/7120>`_.
  - Add CPU capability and BIOS invalid setting checks: see PR `#7216 <https://github.com/projectacrn/acrn-hypervisor/pull/7216>`_.
  - Improve Board Inspector summary and logging based on log levels option: see PR
    `#7429 <https://github.com/projectacrn/acrn-hypervisor/pull/7429>`_.

See the :ref:`board_inspector_tool` documentation for a complete list of steps
to install and run the tool.

Update Configuration Options
============================

In v3.0, data in a launch XML are now merged into the scenario XML for the new
Configurator. When practical, we recommend generating a new scenario and launch
scripts by using the Configurator.

As explained in this :ref:`upgrading_configuration` document, we do provide a
tool that can assist upgrading your existing pre-v3.0 scenario and launch XML
files in the new merged v3.0 format. From there, you can use the v3.0 ACRN
Configurator to open upgraded scenario file for viewing and further editing if the
upgrader tool lost meaningful data during the conversion.

As part of the developer experience improvements to ACRN configuration, the following XML elements
were refined in the scenario XML file:

.. rst-class:: rst-columns3

- ``RDT``
- ``vUART``
- ``IVSHMEM``
- ``Memory``
- ``virtio devices``

The following elements are added to scenario XML files.

.. rst-class:: rst-columns3

- ``vm.lapic_passthrough``
- ``vm.io_completion_polling``
- ``vm.nested_virtualization_support``
- ``vm.virtual_cat_support``
- ``vm.secure_world_support``
- ``vm.hide_mtrr_support``
- ``vm.security_vm``

The following elements were removed.

.. rst-class:: rst-columns3

- ``hv.FEATURES.NVMX_ENABLED``
- ``hv.DEBUG_OPTIONS.LOG_BUF_SIZE``
- ``hv.MEMORY.PLATFORM_RAM_SIZE``
- ``hv.MEMORY.LOW_RAM_SIZE``
- ``hv.CAPACITIES.MAX_IR_ENTRIES``
- ``hv.CAPACITIES.IOMMU_BUS_NUM``
- ``vm.guest_flags``
- ``vm.board_private``

See the :ref:`scenario-config-options` documentation for details about all the
available configuration options in the new Configurator.

In v3.0, we refine the structure of the generated scripts so that PCI functions
are identified only by their BDF. This change serves as a mandatory step to align
how passthrough devices are configured for pre-launched and post-launched VMs.
This allows us to present a unified view in the ACRN Configurator for
assigning passthrough device. We removed some obsolete dynamic parameters and updated the
usage of the Device Model (``acrn-dm``) ``--cpu_affinity`` parameter in launch script generation logic to use the lapic ID
instead of pCPU ID. See :ref:`acrn-dm_parameters-and-launch-script` for details.

Document Updates
****************

With the introduction of the improved Configurator, we could improve our
:ref:`gsg` documentation and let you quickly build a simple ACRN hypervisor and
User VM configuration from scratch instead of using a contrived pre-defined scenario
configuration. That also let us reorganize and change configuration option
documentation to use the newly defined developer-friendly names for
configuration options.

Check out our improved Getting Started and Configuration documents:

.. rst-class:: rst-columns2

* :ref:`introduction`
* :ref:`gsg`
* :ref:`overview_dev`
* :ref:`scenario-config-options`
* :ref:`acrn_configuration_tool`
* :ref:`board_inspector_tool`
* :ref:`acrn_configurator_tool`
* :ref:`upgrading_configuration`
* :ref:`user_vm_guide`
* :ref:`acrn-dm_parameters-and-launch-script`


Here are some of the high-level design documents that were updated since the
v2.7 release:

.. rst-class:: rst-columns2

* :ref:`hld-overview`
* :ref:`atkbdc_virt_hld`
* :ref:`hld-devicemodel`
* :ref:`hld-emulated-devices`
* :ref:`hld-power-management`
* :ref:`hld-security`
* :ref:`hld-virtio-devices`
* :ref:`hostbridge_virt_hld`
* :ref:`hv-cpu-virt`
* :ref:`hv-device-passthrough`
* :ref:`hv-hypercall`
* :ref:`interrupt-hld`
* :ref:`hld-io-emulation`
* :ref:`IOC_virtualization_hld`
* :ref:`virtual-interrupt-hld`
* :ref:`ivshmem-hld`
* :ref:`system-timer-hld`
* :ref:`uart_virtualization`
* :ref:`virtio-blk`
* :ref:`virtio-console`
* :ref:`virtio-input`
* :ref:`virtio-net`
* :ref:`vuart_virtualization`
* :ref:`l1tf`
* :ref:`trusty_tee`

We've also made edits throughout the documentation to improve clarity,
formatting, and presentation.  We started updating feature enabling tutorials
based on the new Configurator, and will continue updating them after the v3.0
release (in the `latest documentation <https://docs.projectacrn.org>`_).

.. rst-class:: rst-columns2

* :ref:`develop_acrn`
* :ref:`doc_guidelines`
* :ref:`acrn_doc`
* :ref:`hardware`
* :ref:`acrn_on_qemu`
* :ref:`cpu_sharing`
* :ref:`enable_ivshmem`
* :ref:`enable-s5`
* :ref:`gpu-passthrough`
* :ref:`inter-vm_communication`
* :ref:`rdt_configuration`
* :ref:`rt_performance_tuning`
* :ref:`rt_perf_tips_rtvm`
* :ref:`using_hybrid_mode_on_nuc`
* :ref:`using_ubuntu_as_user_vm`
* :ref:`using_windows_as_uos`
* :ref:`vuart_config`
* :ref:`acrnshell`
* :ref:`hv-parameters`
* :ref:`debian_packaging`
* :ref:`acrnctl`

Some obsolete documents were removed from the v3.0 documentation, but can still
be found in the archived versions of previous release documentation, such as for
`v2.7 <https://docs.projectacrn.org/2.7/>`_.


Fixed Issues Details
********************

.. comment example item
   - :acrn-issue:`5626` - Host Call Trace once detected

- :acrn-issue:`7712` - [config_tool] Make Basic tab the default view
- :acrn-issue:`7657` - [acrn-configuration-tool] Failed to build acrn with make BOARD=xx SCENARIO=shared RELEASE=y
- :acrn-issue:`7641` - config-tools: No launch scripts is generated when clicking save
- :acrn-issue:`7637` - config-tools: vsock refine
- :acrn-issue:`7634` - [DX][TeamFooding][ADL-S] Failed to build acrn when disable multiboot2 in configurator
- :acrn-issue:`7623` - [config_tool] igd-vf para is no longer needed in launch script
- :acrn-issue:`7609` - [Config-Tool][UI]build acrn failed after delete VMs via UI
- :acrn-issue:`7606` - uninitialized variables are used in hpet.c
- :acrn-issue:`7597` - [config_tool] Use Different Board - delete old board file
- :acrn-issue:`7592` - [acrn-configuration-tool] Hide PCI option for Console virtual UART type
- :acrn-issue:`7581` - [ADL-S][shared]The SOS cmdline parameter shouldn't be added manually and should be changed in Debian package.
- :acrn-issue:`7571` - [config_tool] Working folder with my_board.xml behavior
- :acrn-issue:`7563` - [ADL-S][SSRAM]RTCM Unit run failed with 2G memory size
- :acrn-issue:`7556` - [config_tool] VUART is configured to PCI and generate launch script without relevant parameters
- :acrn-issue:`7546` - [acrn-configuration-tool]Scenario files generated with acrn-configurator for boards without serial ports ACRN debug build fails
- :acrn-issue:`7540` - [config_tool]: rename virtio console as virtio serial port(as console)
- :acrn-issue:`7538` - configurator: CPU Affinity should be hidden from service vm
- :acrn-issue:`7535` - config-tools: add vhost vsock in v3.0
- :acrn-issue:`7532` - [config_tool] dialog box and deleting VM related issues
- :acrn-issue:`7530` - [configurator] Maximum Virtual CLOS configuration value should not allow negative numbers
- :acrn-issue:`7526` - [configurator] Assigning one cpu to multiple Pre-launched VMs is not reported as error
- :acrn-issue:`7519` - [config_tool] Duplicate VM name
- :acrn-issue:`7514` - fix FEATURES to restore basic view
- :acrn-issue:`7506` - config-tools: configurator widget vuart connection needs validation
- :acrn-issue:`7500` - [config_tool] Failed to delete post-launched VM due to IVSHMEM
- :acrn-issue:`7498` - board_inspector.py fails to run on target with clean Ubuntu installation
- :acrn-issue:`7495` - [config_tool] Starting new configuration deletes all files in existing working folder
- :acrn-issue:`7492` - configurator: fix configurator build issue
- :acrn-issue:`7488` - Configurator version confusion
- :acrn-issue:`7486` - [config_tool] Duplicate VM name
- :acrn-issue:`7484` - configurator: User-input working directory not working
- :acrn-issue:`7481` - [config_tool] No validation for required fields in widgets
- :acrn-issue:`7470` - [config_tool][UI] scenario.xml is still generated even though there are setting errors
- :acrn-issue:`7469` - [config_tool][UI] No promption on save button if there are wrong settings
- :acrn-issue:`7455` - configurator: vUART widget not working
- :acrn-issue:`7450` - config-tools: bugfix for file related issues in UI
- :acrn-issue:`7445` - [Config-Tool][UI]The shared VMs_name for IVSHMEM is not consistent with the VM_name modification
- :acrn-issue:`7442` - [config_tool] Tooltip runs off screen
- :acrn-issue:`7435` - configurator: Steps should be inactive until prior step complete
- :acrn-issue:`7425` - Cache was not locked after post-RTVM power off and restart
- :acrn-issue:`7424` - [config_tool] Virtual USB HCI should be a dropdown menu
- :acrn-issue:`7421` - configurator: Unable to display PCI devices droplist
- :acrn-issue:`7420` - configurator: Unable to set physical CPU affinity
- :acrn-issue:`7419` - configurator: prelaunched VM assigned wrong VMID
- :acrn-issue:`7418` - configurator: open folder path incorrect
- :acrn-issue:`7413` - config-tools: bugfix for UI
- :acrn-issue:`7402` - [acrn-deb] install board_inspector overwrites grub cmdline
- :acrn-issue:`7401` - Post-RTVM boot failure with SSRAM enabled
- :acrn-issue:`7400` - [acrn-configuration-tool][acrn-deb] grub is not update correctly after install the acrn-deb
- :acrn-issue:`7392` - There is no virtio_devices node in generic scenario xml
- :acrn-issue:`7383` - [acrn-configuration tool] make scenario shared file error cp error
- :acrn-issue:`7376` - Virtio-GPU in guest_vm fails to get the EDID
- :acrn-issue:`7370` - [acrn-deb] install_compile_package function is not consistent with gsg
- :acrn-issue:`7366` - [acrn-configuration tool] make scenario shared file error cp error
- :acrn-issue:`7365` - [config_tool] Get errors running board_inspector
- :acrn-issue:`7361` - config_tool: Add check for RTVM pCPU assignment
- :acrn-issue:`7356` - [UI]  Board info not updated when user changed the board XML
- :acrn-issue:`7349` - [UI]Not delete all VMs while delete Service_VM on UI
- :acrn-issue:`7345` - Build will fail when using absolute path
- :acrn-issue:`7337` - Memory leak after creating udmabuf for virtio-gpu zero_copy
- :acrn-issue:`7330` - [PCI UART] Fail to build hypervisor without pci uart bdf value
- :acrn-issue:`7327` - refine pgentry_present field in struct pgtable
- :acrn-issue:`7301` - [Virtio-GPU]Not enough free memory reserved in SOS
- :acrn-issue:`7298` - boot time issue for acrn-dm
- :acrn-issue:`7297` - update parameter in schema
- :acrn-issue:`7296` - Segment fault is triggered in course of Virtio-gpu rebooting test
- :acrn-issue:`7270` - combined cpu_affinity warning for service vm
- :acrn-issue:`7267` - service vm cpu affinity issue
- :acrn-issue:`7265` - About 20s after booting uaag with usb mediator,usb disk isn't recognized
- :acrn-issue:`7261` - Hide PTM in Configurator UI
- :acrn-issue:`7256` - Remove SCHED_IORR and KERNEL_RAWIMAGE
- :acrn-issue:`7249` - doc: Exception in Sphinx processing doesn't display error message
- :acrn-issue:`7248` - restore copyright notice of original author of some files.
- :acrn-issue:`7246` - Can't fully parse the xml content that was saved by the same version configurator
- :acrn-issue:`7241` - need copyright notice and license in virtio_gpu.c
- :acrn-issue:`7212` - Exception"not enough space in guest VE820 SSRAM area" showed when built ACRN with RTCT table
- :acrn-issue:`7208` - iasl segfault when reboot user VM with virtio-i2c devices
- :acrn-issue:`7197` - The error message is found after adding the specific mac address in the launch script
- :acrn-issue:`7172` - [acrn-configuration-tool] offline_cpus won't be executed in NOOP mode
- :acrn-issue:`7171` - Link to instead of including old release notes in the current release
- :acrn-issue:`7159` - acrn-config: config tool get_node apic_id failed
- :acrn-issue:`7136` - [acrn-configuration-tool] Share memory should never support 512M since the HV_RAM_SIZE_MAX is limited to 0x40000000 but not a platform specific problem
- :acrn-issue:`7133` - Guest VM system reset may fail and ACRN DM program hang
- :acrn-issue:`7127` - config-tools: remove SERIAL_CONSOLE extracion for bootargs of SOS
- :acrn-issue:`7124` - [DM]:  Fail to boot the Laag guest if the boot option of "pci=nomsi" is added for Guest kernel
- :acrn-issue:`7119` - config-tools: bring all cores online
- :acrn-issue:`7109` - Python traceback if the 'dpkg' tool is not available
- :acrn-issue:`7098` - Memory leakage bug induced by opendir() in ACRN applications
- :acrn-issue:`7084` - config_tools:  append passthrough gpu bdf in hexadecimal format
- :acrn-issue:`7077` - config-tools: find pci hole based on all pci hostbridge
- :acrn-issue:`7058` - config_tools: board_inspector cannot generate compliable xml for Qemu
- :acrn-issue:`7045` - Segmentation fault when passthrough TSN to post_launched VM with enable_ptm option
- :acrn-issue:`7022` - ACRN debian package not complete when source is not cloned to standard folder
- :acrn-issue:`7018` - No expected exception generated on some platform.


Known Issues
************

- :acrn-issue:`6631` - [KATA] Kata support is broken since v2.7
- :acrn-issue:`6978` - openstack failed since ACRN v2.7
- :acrn-issue:`7827` - [Configurator] Pre_launched standard VMs cannot share CPU with Service VM
- :acrn-issue:`7831` - [Configurator] Need to save twice to generate vUART and IVSHMEM addresses

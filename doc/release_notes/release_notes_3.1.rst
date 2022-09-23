.. _release_notes_3.1:

ACRN v3.1 (Sep 2022)
####################

We are pleased to announce the release of the Project ACRN hypervisor
version 3.1.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline
embedded development through an open-source platform. See the
:ref:`introduction` introduction for more information.

All project ACRN source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, tools, and
documentation. You can download this source code either as a zip or
tar.gz file (see the `ACRN v3.1 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v3.1>`_) or
use Git ``clone`` and ``checkout`` commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v3.1

The project's online technical documentation is also tagged to
correspond with a specific release: generated v3.1 documents can be
found at https://projectacrn.github.io/3.1/.  Documentation for the
latest development branch is found at https://projectacrn.github.io/latest/.

ACRN v3.1 requires Ubuntu 20.04.  Follow the instructions in the
:ref:`gsg` to get started with ACRN.


What's New in v3.1
******************

More ACRN Configuration Improvements
  Release v3.0 featured a new ACRN Configurator UI tool with a more intuitive
  design and workflow that simplifies getting the setup for the ACRN hypervisor
  right. With this v3.1 release, we've continued making improvements to the
  Configurator including more comprehensive error checking with more
  developer-friendly messages.  You'll also see additional advanced
  configuration settings for tuning real-time performance including Cache
  Allocation Technology (CAT) and vCPU affinity.  Read more in the
  :ref:`acrn_configurator_tool` and :ref:`scenario-config-options` documents.

  If you have feedback on this, or other aspects of ACRN, please share them on
  the `ACRN users mailing list <https://lists.projectacrn.org/g/acrn-users>`_.

  As with the v3.0 release, we've simplified installation of the Configurator by providing a Debian
  package that you can download from the `ACRN v3.1 tag assets
  <https://github.com/projectacrn/acrn-hypervisor/releases/download/v3.1/acrn-configurator-3.1.deb>`_
  and install.  See the :ref:`gsg` for more information.

Improved Board Inspector Collection and Reporting
  You run the ACRN Board Inspector tool to collect information about your target
  system's processors, memory, devices, and more. The generated board XML file
  is used by the ACRN Configurator to determine which ACRN configuration options
  are possible, as well as possible values for target system resources. The v3.1
  Board Inspector has improved scanning and provides more messages about
  potential issues or limitations of your target system that could impact ACRN
  configuration options.

  The Board Inspector is updated to probe beyond CPUID
  information for Cache Allocation Technology (CAT) support and also detects
  availability of L3 CAT by accessing the CAT MSRs directly. Read more in
  :ref:`board_inspector_tool`.

Sample Application with Two Post-Launched VMs
  With this v3.1 release, we provide a follow-on :ref:`GSG_sample_app` to the
  :ref:`gsg`.  This sample application shows how to create two VMs that are
  launched on your target system running ACRN.  One VM is a real-time VM running
  `cyclictest
  <https://wiki.linuxfoundation.org/realtime/documentation/howto/tools/cyclictest/start>`__,
  an open-source application commonly used to measure latencies in real-time
  systems. This real-time VM (RT_VM) uses inter-VM shared memory (IVSHMEM) to
  send data to a second Human-Machine Interface VM (HMI_VM) that formats and
  presents the collected data as a histogram on a web page shown by a browser.
  This guide shows how to configure, create, and launch the two VM images that
  make up this application. Full code for the sample application is provided in
  the ``acrn-hypervisor`` GitHub repo :acrn_file:`misc/sample_application`.

Multiple-Displays Support for VMs
  The virtio-gpu mechanism is enhanced to support VMs with multiple displays.

Improved TSC Frequency Reporting
  The hypervisor now reports Time Stamp Counter (TSC) frequency in KHz so that
  VMs can get that number without calibrating to a high precision timer.

Upgrading to v3.1 from Previous Releases
****************************************

We recommend you generate a new board XML for your target system with the v3.1
Board Inspector. You should also use the v3.1 Configurator to generate a new
scenario XML file and launch scripts. Scenario XML files and launch scripts
created by previous ACRN versions will not work with the v3.1 ACRN hypervisor
build process and could produce unexpected errors during the build.

Given the scope of changes for the v3.1 release, we have recommendations for how
to upgrade from prior ACRN versions:

1. Start fresh from our :ref:`gsg`. This is the best way to ensure you have a
   v3.1-ready board XML file from your target system and generate a new scenario
   XML and launch scripts from the new ACRN Configurator that are consistent and
   will work for the v3.1 build system.
#. Use the :ref:`upgrader tool <upgrading_configuration>` to attempt upgrading
   configuration files that worked with a release before v3.1. You'll need the
   matched pair of scenario XML and launch XML files from a prior configuration,
   and use them to create a new merged scenario XML file. See
   :ref:`upgrading_configuration` for details.
#. Manually edit your prior scenario XML and launch XML files to make them
   compatible with v3.1.  This is not our recommended approach.

Here are some additional details about upgrading to the v3.1 release.

Generate New Board XML
======================

Board XML files, generated by ACRN Board Inspector, contain board information
that is essential for building the ACRN hypervisor and setting up User VMs.
Compared to previous versions, ACRN v3.1 adds the following information to the board
XML file for supporting new features and fixes:

* Add a progress bar and timeout mechanism to the Board Inspector
* Guess L3 CAT parameters if not reported via CPUID
* Refactor MSR utilities
* Record all details from RTCT in the board XML and generate vRTCT instead of
  copying a physical one
* Hide unnecessary logs and fix typos in error messages

See the :ref:`board_inspector_tool` documentation for a complete list of steps
to install and run the tool.

Update Configuration Options
============================

As explained in this :ref:`upgrading_configuration` document, we do provide a
tool that can assist upgrading your existing pre-v3.1 scenario XML files in the
new merged v3.1 format. From there, you can use the v3.1 ACRN Configurator to
open the upgraded scenario file for viewing and further editing if the upgrader 
tool lost meaningful data during the conversion.

As part of the developer experience improvements to ACRN configuration, the
following XML elements were refined in the scenario XML file:

* ``ENFORCE_TURNOFF_AC`` is now ``SPLIT_LOCK_DETECTION_ENABLED``.
* ``ENFORCE_TURNOFF_GP`` is now ``UC_LOCK_DETECTION_ENABLED``.
* ``MCE_ON_PSC_DISABLED`` is now ``MCE_ON_PSC_ENABLED``.

See the :ref:`scenario-config-options` documentation for details about all the
available configuration options in the new Configurator.


Document Updates
****************

Sample Application User Guide
   The new :ref:`GSG_sample_app` documentation shows how to configure, build, and
   run a practical application with a Real-Time VM and Human-Machine Interface
   VM that communicate using inter-VM shared memory.


We've also made edits throughout the documentation to improve clarity,
formatting, and presentation.  We started updating feature enabling tutorials
based on the new Configurator, and will continue updating them after the v3.1
release (in the `latest documentation <https://docs.projectacrn.org>`_). Here
are some of the more significant updates:

.. rst-class:: rst-columns2

* :ref:`gsg`
* :ref:`rdt_configuration`
* :ref:`acrn-dm_parameters-and-launch-script`
* :ref:`scenario-config-options`
* :ref:`hv-hypercall`
* :ref:`hardware`
* :ref:`cpu_sharing`
* :ref:`enable-s5`
* :ref:`using_grub`
* :ref:`vuart_config`
* :ref:`acrnshell`
* :ref:`acrnctl`

Fixed Issues Details
********************

.. comment example item
   - :acrn-issue:`5626` - Host Call Trace once detected

- :acrn-issue:`8162` -	 dm: virtio-blk parameter error
- :acrn-issue:`8125` -	 [hypercube][ADL]Assertion'0' failed found during hypercube_PIO_SCAN testing
- :acrn-issue:`8111` -	 [life_mngr] Sync between SOS and RTVM failed when startup hence life_mngr cannot work
- :acrn-issue:`7948` -	 Tiger Lake product with CAT enabled needed
- :acrn-issue:`8063` -	 Need to generate config_summary.rst
- :acrn-issue:`8098` -	 configurator build has dependency on published release documentation
- :acrn-issue:`8087` -	 ACRN Configurator enable RDT and CDP after launch scenario
- :acrn-issue:`8066` -	 User VM doesn't have IP if launched by acrnd with virtio-net
- :acrn-issue:`7563` -	 [ADL-S][SSRAM]RTCM Unit run failed with 2G memory size
- :acrn-issue:`8068` -	 [config_tool] Imported scenarios not populating CAT widget
- :acrn-issue:`7973` -	 config-tools: User concern about the progress of generating board.xml and not sure whether need exit after waiting some time.
- :acrn-issue:`7975` -	 GSG python package xmlschema/elementpath incompatibility
- :acrn-issue:`8050` -	 config_tools:  cpu affinity
- :acrn-issue:`8051` -	 ADL-S][S5]Can't shutdown WaaG with user VM name same as in launch script
- :acrn-issue:`8046` -	 [config_tool] Configurator creates duplicate VM name
- :acrn-issue:`8018` -	 config-tools: default value & MAX_PCI_BUS_NUM
- :acrn-issue:`7991` -	 [Workshop] Configurator create scenario popup window disappear sometimes.
- :acrn-issue:`8033` -	 [config_tool] vUART widget address for pre-launched VM doesn't update or display error
- :acrn-issue:`8000` -	 [config_tool] Real-time vCPU checkbox is confusing to users
- :acrn-issue:`7898` -	 [config_tool] Warning message when users attempt to create a new scenario or import an existing scenario for an existing configuration
- :acrn-issue:`5692` -	 Keep Open: Used when updating config option documentation in .xsd files
- :acrn-issue:`7914` -	 [config_tool] Focus change to new VM when added
- :acrn-issue:`7661` -	 config-tools: Change MCE AC and UC (GP) to enable
- :acrn-issue:`7927` -	 [acrn-configuration-tool] ACRN-Configurator does not consider L3 CAT config when opening an exiting configuration
- :acrn-issue:`7958` -	 [configurator] Need highlight success or fail status in the popup window of save scenario button
- :acrn-issue:`7913` -	 config-tools: build acrn successfully even set the same memory address between the pre vm 0 and hv
- :acrn-issue:`7931` -	 Improve HV console as a bash-like one
- :acrn-issue:`7960` -	 dm cannot stop correctly
- :acrn-issue:`7935` -	 Build EFI-Stub fail
- :acrn-issue:`7921` -	 [config_tool] Cache widget: Instructions missing for L2-only users
- :acrn-issue:`7925` -	 [configurator] invalid virtual bdf in ivshmem could be saved
- :acrn-issue:`7942` -	 [config_tool] vUART and IVSHMEM widgets don't generate address
- :acrn-issue:`7947` -	 Need optimize user flow to configure SSRAM for pre-launched RTVM
- :acrn-issue:`7944` -	 Service VM hang when reboot user VM
- :acrn-issue:`7940` -	 DM: add support for iothread
- :acrn-issue:`7933` -	 [config_tool] Add tooltips to CPU affinity virtio
- :acrn-issue:`7926` -	 [acrn-configuration-tool]Memory warning when build ACRN for WHL
- :acrn-issue:`7902` -	 coding style cleanup in pci.c
- :acrn-issue:`7917` -	 [config_tool] Order of plus and minus icons is inconsistent
- :acrn-issue:`7790` -	 User VM fail to get IP with vhost net
- :acrn-issue:`7759` -	 ERROR: LeakSanitizer: detected memory leaks found when run 
- :acrn-issue:`7915` -	 [config_tool] Duplicate VM error message - add spacing
- :acrn-issue:`7906` -	 config-tools: Board inspector crashed when command line is too long in dmesg
- :acrn-issue:`7907` -	 RRSBA should be disabled on platform using retpoline if enumerated
- :acrn-issue:`7897` -	 [config_tool] Placement of plus icon for virtio
- :acrn-issue:`7582` -	 configurator: board name truncated incorrectly
- :acrn-issue:`7707` -	 configurator: IVSHMEM region name lacks pattern check
- :acrn-issue:`7886` -	 config-tools: need to hide the 'update-pciids: download failed' message
- :acrn-issue:`7559` -	 Copyright year range in code headers is not up to date
- :acrn-issue:`7884` -	 config_tool: Move "BIOS Revision" line in Configurator UI
- :acrn-issue:`7893` -	 [config_tool] Incompatibility with latest elementpath library
- :acrn-issue:`7887` -	 hv: Surges of external interrupts may cause guest hang
- :acrn-issue:`7880` -	 fail to run acrn-dm with wrong iasl path

Known Issues
************

- :acrn-issue:`6631` -	[KATA] Kata support is broken since v2.7
- :acrn-issue:`6978` -	openstack failed since ACRN v2.7
- :acrn-issue:`7827` -	[Configurator] Pre_launched standard VMs cannot share CPU with Service VM
- :acrn-issue:`8202` -	[HV][qemu0][v3.1]HV fail to boot acrn on qemu


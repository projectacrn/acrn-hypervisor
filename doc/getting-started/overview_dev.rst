.. _overview_dev:

Configuration and Development Overview
######################################

This overview is for developers who are new to ACRN and are responsible for
configuring and building the hypervisor and the VMs for applications. It will
introduce you to the general development process, including ACRN components and
tools.

The overview covers the process at an abstract and universal level.

* Abstract: the overall structure rather than detailed instructions
* Universal: applicable to most use cases

This overview complements the :ref:`gsg`. The guide provides
step-by-step instructions to enable an ACRN example for first-time use, while
the overview provides background information and serves as a gateway to
additional features and resources that can help you develop your solution.

See :ref:`introduction` for information about ACRN benefits, use cases, and
architecture.

.. _overview_dev_dev_env:

Development Environment
***********************

The recommended development environment for ACRN consists of two machines:

* **Development computer** where you configure and build ACRN images
* **Target system** where you install and run ACRN images

.. image:: ./images/overview_host_target.png

ACRN requires a serial output from the target system to the development computer
for :ref:`debugging and system messaging <acrn-debug>`. If your target doesn't
have a serial output, :ref:`here are some tips for connecting a serial output
<connect_serial_port>`.

You need a way to copy the built ACRN images and other files between the
development computer and target system. ACRN documentation, such as the
:ref:`gsg`, offers steps for copying via USB disk as a simple solution.

General Process for Building an ACRN Hypervisor
***********************************************

The general process for configuring and building an ACRN hypervisor is
illustrated in the following figure. Additional details follow.

.. image:: ./images/overview_flow-1-0.6x.png

.. _overview_dev_hw_scenario:

|icon_light| Step 1: Select Hardware and Scenario
*************************************************

.. |icon_light| image:: ./images/icon_light.png

ACRN configuration is hardware and scenario specific. You will need to learn
about supported ACRN hardware and scenarios, and select the right ones for your
needs.

Select Your Hardware
====================

ACRN supports certain Intel processors. Development kits are widely available.
See :ref:`hardware`.

.. _overview_dev_select_scenario:

Select Your Scenario
====================

A scenario defines a specific ACRN configuration, such as hypervisor
capabilities, the type and number of VMs that can be run, their attributes, and
the resources they have access to.

This image shows an example of an ACRN scenario to illustrate the types of VMs
that ACRN offers:

.. image:: ./images/ACRN_terms-1-0.75x.png

ACRN offers three types of VMs:

* **Pre-launched User VMs**: These VMs run independently of other VMs and own
  dedicated hardware resources, such as CPU cores, memory, and I/O devices.
  Other VMs, including the Service VM, may not even be aware of a pre-launched
  VM's existence. The configuration of pre-launched VMs is static and must be
  defined at build time. They are well-suited for safety-critical applications
  and where very strict isolation, including from the Service VM, is desirable.

* **Service VM**: A special VM, required for scenarios that have post-launched
  User VMs. The Service VM can access hardware resources directly by running
  native drivers and provides device sharing services to post-launched User VMs
  through the :ref:`ACRN Device Model (DM) <hld-devicemodel>` ``acrn-dm``
  application. The Device Model runs inside the Service VM and is responsible
  for creating and launching a User VM and then performing device emulation for
  the devices configured for sharing with that User VM. ACRN supports one
  Service VM.

* **Post-launched User VMs**: These VMs typically share hardware resources via
  the Service VM and Device Model. They can also access hardware devices
  directly if they've been configured as passthrough devices. The configuration
  of a post-launched VM can be static (defined at build time) or dynamic
  (defined at runtime without rebuilding ACRN). They are well-suited for
  non-safety applications, including human machine interface (HMI), artificial
  intelligence (AI), computer vision, real-time, and others.

The names "pre-launched" and "post-launched" refer to the boot order of these
VMs. The ACRN hypervisor launches the pre-launched VMs first, then launches the
Service VM. The Service VM launches the post-launched VMs.

Pre-launched VMs are recommended only if you need complete isolation from the
rest of the system. Most use cases can meet their requirements without
pre-launched VMs. Even if your application has stringent real-time requirements,
start by testing the application on a post-launched VM before considering a
pre-launched VM.

Scenario Types
---------------

ACRN categorizes scenarios into :ref:`three types <usage-scenarios>`:

* **Shared scenario:** This scenario represents a traditional computing, memory,
  and device resource sharing model among VMs. It has post-launched User VMs and
  the required Service VM. There are no pre-launched VMs in this scenario.

* **Partitioned scenario:** This scenario has pre-launched User VMs only. It
  demonstrates VM partitioning: the User VMs are independent and isolated, and
  they do not share resources. For example, a pre-launched VM may not share a
  storage device with any other VM, so each pre-launched VM requires its own
  boot device. There is no need for the Service VM or Device Model because all
  partitioned VMs run native device drivers and directly access their configured
  resources.

* **Hybrid scenario:** This scenario simultaneously supports both sharing and
  partitioning on the consolidated system. It has pre-launched VMs and
  post-launched VMs, along with the Service VM.

While designing your scenario, keep these concepts in mind as you will see them
mentioned in ACRN components and documentation.

|icon_host| Step 2: Prepare the Development Computer
****************************************************

.. |icon_host| image:: ./images/icon_host.png

Your development computer requires certain dependencies to configure and build
ACRN:

* Ubuntu OS (ACRN development is not supported on Windows.)
* Build tools
* ACRN hypervisor source code
* If your scenario has a Service VM: ACRN kernel source code

.. _overview_dev_board_config:

|icon_target| Step 3: Generate a Board Configuration File
*********************************************************

.. |icon_target| image:: ./images/icon_target.png

The :ref:`board_inspector_tool`, found in the ACRN
hypervisor source code, enables you to generate a board configuration file on
the target system.

A **board configuration file** stores hardware-specific information extracted
from the target system. This XML file describes the capacity of hardware
resources (such as processors and memory), platform power states, available
devices, and BIOS settings. The file is used to configure and build the ACRN
hypervisor, because each hypervisor instance is specific to your target
hardware.

The following sections provide an overview and important information to keep
in mind when using the Board Inspector.

Configure BIOS Settings
=======================

You must configure all of your target's BIOS settings before running the Board
Inspector tool, because the tool records the current BIOS settings in the board
configuration file.

ACRN requires the BIOS settings listed in :ref:`gsg-board-setup` of
the Getting Started Guide.

Use the Board Inspector to Generate a Board Configuration File
==============================================================

The Board Inspector requires certain dependencies to be present on the target
system:

* Ubuntu OS
* Tools and kernel command-line options that allow the Board Inspector to
  collect information about the target hardware

After setting up the dependencies, you run the Board Inspector via command-line.
The tool generates the board configuration file specific to your hardware.

.. important:: Whenever you change the configuration of the board, such as BIOS
   settings or PCI ports, you must generate a new board configuration file.

You will need the board configuration file in :ref:`overview_dev_config_editor`
and :ref:`overview_dev_build`.

.. _overview_dev_config_editor:

|icon_host| Step 4: Generate a Scenario Configuration File and Launch Scripts
*****************************************************************************

The :ref:`acrn_configurator_tool` lets you configure your scenario settings via
a graphical user interface (GUI) on your development computer.

The tool imports the board configuration file that you generated in
:ref:`overview_dev_board_config`. Then you can configure your scenario, such as
set hypervisor capabilities, add VMs, modify their attributes, and delete VMs.
The tool validates your inputs against your board configuration file to ensure
the scenario is supported by the target hardware. The tool saves your settings
to a **scenario configuration file** in XML format. You will need this file in
:ref:`overview_dev_build`.

If your scenario configuration has post-launched User VMs, the tool also
generates a **launch script** for each of those VMs. The launch script contains
the settings needed to launch the User VM and emulate the devices configured for
sharing with that User VM. You will run this script in the Service VM in
:ref:`overview_dev_install`.

.. _overview_dev_build:

|icon_host| Step 5: Build ACRN
******************************

The ACRN hypervisor source code provides a makefile to build the ACRN hypervisor
binary and associated components. In the ``make`` command, you need to specify
your board configuration file and scenario configuration file. The build
typically takes a few minutes.

If your scenario has a Service VM, you also need to build the ACRN kernel for
the Service VM. The ACRN kernel source code provides a predefined configuration
file and a makefile to build the ACRN kernel binary and associated components.
The kernel build can take 15 minutes or less on a fast computer, but could take
an hour or more depending on the performance of your development computer.

.. _overview_dev_install:

|icon_target| Step 6: Install and Run ACRN
******************************************

The last step is to make final changes to the target system configuration and
then boot ACRN.

At a high level, you will:

* Copy the built ACRN hypervisor files, Service VM kernel files, and launch
  scripts from the development computer to the target.

* Configure GRUB to boot the ACRN hypervisor, pre-launched VMs, and Service VM.
  Reboot the target, and launch ACRN.

* If your scenario contains a post-launched User VM, install an OS image for the
  post-launched VM and run the launch script you created in
  :ref:`overview_dev_config_editor`. The script invokes the Service VM's Device
  Model to create the User VM.

Learn More
**********

* To get ACRN up and running for the first time, see the :ref:`gsg` for
  step-by-step instructions.

* If you have already completed the :ref:`gsg` , see the :ref:`develop_acrn` for
  more information about configuring and debugging ACRN.

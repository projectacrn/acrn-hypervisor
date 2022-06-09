.. _acrn_configuration_tool:

Introduction to ACRN Configuration
##################################

ACRN configuration is designed for System Integrators and Tier 1s to customize
ACRN to meet their own needs. It allows users to adapt ACRN to target boards as
well as configure hypervisor capabilities and provision VMs.

ACRN configuration consists of the following key components.

* Configuration data saved as XML files.
* A configuration toolset to generate and edit configuration data.

The following sections introduce the concepts and tools of ACRN configuration
from the aspects below.

* :ref:`acrn_config_types` introduces the objectives and main contents of
  different types of configuration data.
* :ref:`acrn_config_workflow` overviews the steps to customize ACRN
  configuration using the configuration toolset.

.. _acrn_config_types:

Types of Configurations
***********************

ACRN includes two types of configurations: board and scenario. The
configuration data are saved in XML files. The following sections briefly
describe the objectives and main contents of each file.

Board Configuration File
========================

The board configuration file stores hardware-specific information extracted
from the target platform. Examples of information:

* Capacity of hardware resources (such as processors and memory)
* Platform power states
* Available devices
* BIOS versions

You need a board configuration file to create scenario configurations. The
board configuration is scenario-neutral by nature. Thus, multiple scenario
configurations can be based on the same board configuration.

You also need a board configuration file to build an ACRN hypervisor. The
build process uses the file to build a hypervisor that can
initialize and manage the platform at runtime.

Scenario Configuration File
===========================

The scenario configuration file defines a working scenario by configuring
hypervisor capabilities and defining VM attributes and resources. Examples of
parameters:

* Hypervisor capabilities

  - Availability and settings of hypervisor features, such as debugging
    facilities, scheduling algorithm, inter-VM shared memory (ivshmem),
    and security features.
  - Hardware management capacity of the hypervisor, such as maximum PCI devices
    and maximum interrupt lines supported.
  - Memory consumption of the hypervisor, such as the entry point and stack
    size.

* VM attributes and resources

  - VM attributes, such as VM names.
  - Maximum number of VMs supported.
  - Resources allocated to each VM, such as number of vCPUs, amount of guest
    memory, and pass-through devices.
  - User VM settings, such as boot protocol and VM OS kernel parameters.
  - Settings of virtual devices, such as virtual UARTs.

You need a scenario configuration file to build an ACRN hypervisor. The build process uses the file to build a hypervisor that can initialize its capabilities and set up the VMs at runtime.

For pre-launched User VMs, all attributes and resources are static
configurations. The VM attributes and resources are exactly the amount of
resources allocated to them.

For post-launched User VMs, some resources are static configurations. Other
resources are under the control of the Service VM and can be dynamically
allocated to a VM via a launch script.

.. _acrn_config_workflow:

Using ACRN Configuration Toolset
********************************

The ACRN configuration toolset lets you create and edit configuration data. The
toolset includes:

* :ref:`Board Inspector <board_inspector_tool>`: Collects information from your
  target machine and generates a board configuration file.
* :ref:`ACRN Configurator <acrn_configurator_tool>`: Provides a graphical user
  interface (GUI) for configuring your hypervisor and VM parameters, and
  generates a scenario configuration file and launch scripts.

As introduced in :ref:`overview_dev`, configuration takes place at
:ref:`overview_dev_board_config` and :ref:`overview_dev_config_editor` in
the overall development process:

.. image:: ../getting-started/images/overview_flow-1-0.6x.png

ACRN source also includes makefile targets to aid customization. See
:ref:`hypervisor-make-options`.

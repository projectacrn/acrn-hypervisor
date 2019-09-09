.. _acrn_configuration_tool:

ACRN Configuration Tool
#######################

.. note:: This document is under development and planned for the 1.3
   release.


ACRN Configurations Introduction
********************************
There are three types of configurations in ACRN: Hypervisor,
Board, and VM. We'll explore each of these in the following sections.

Hypervisor configuration
========================
Hypervisor configuration selects a working scenario and target
board by configuring the hypervisor image features and capabilities such as
setting up the log and the serial port.

Hypervisor configuration is done using the ``Kconfig`` ``make
menuconfig`` mechanism.  The configuration file is located at::

   acrn-hypervisor/hypervisor/arch/x86/configs/Kconfig

A board-specific ``defconfig`` file, located at::

   acrn-hypervisor/hypervisor/arch/x86/configs/$(BOARD)/$(BOARD).config

will be loaded first, as the default ``Kconfig`` for the specified board.

Board configuration
===================
The board configuration stores board-specific settings referenced by the
ACRN hypervisor. This includes *scenario-relevant* information such as
board settings, root device selection, and kernel cmdline, and
*scenario-irrelevant** hardware-specific information such as ACPI/PCI
and BDF information.  The board configuration is organized as
``*.c/*.h`` files located at::

   acrn-hypervisor/hypervisor/arch/x86/$(BOARD)/

VM configuration
=================
VM configuration includes *scenario-based* VM configuration
information, used to describe the characteristics and attributes for VMs
on each user scenario, and *launch script-based* VM configuration, where
parameters are passed to the device model to launch post-launched User
VMs.

Scenario based VM configurations are organized
as ``*.c/*.h`` files located at::

   acrn-hypervisor/hypervisor/scenarios/$(SCENARIO)/

User VM launch script samples are located at::

   acrn-hypervisor/devicemodel/samples/

Configuration tool workflow
***************************

Hypervisor configuration workflow
==================================
Hypervisor configuration is based on the ``Kconfig`` ``make menuconfig``
mechanism.  You begin by creating a board specific ``defconfig`` file to
set up the default ``Kconfig`` values for the specified board.
Then you configure the hypervisor build options using the ``make
menuconfig`` graphical interface.  The resulting ``.config`` file is
used by the ACRN build process to create a configured scenario- and
board-specific hypervisor image.

.. figure:: images/sample_of_defconfig.png
   :align: center

   defconfig file sample

.. figure:: images/GUI_of_menuconfig.png
   :align: center

   menuconfig interface sample

Please refer to the :ref:`getting-started-hypervisor-configuration` for
detailed steps.

Board and VM configuration workflow
===================================
Python offline tools are provided to configure Board and VM configurations.
The tool source folder is located at::

   acrn-hypervisor/misc/acrn-config/

Here is the offline configuration tool workflow:

#. Get board info.

   a. Set up native Linux environment on target board.
   #. Copy ``target`` folder into target file system and then run
      ``sudo python3 board_parser.py $(BOARD)`` command.
   #. A $(BOARD).xml that includes all needed hardware-specific information
      will be generated in the ``./out/`` folder. (Here ``$(BOARD)`` is the
      specified board name)

      | **Native Linux requirement:**
      | **Release:** Ubuntu 18.04+ or Clear Linux 30210+
      | **Tools:** cpuid, rdmsr, lspci, dmidecode (optional)
      | **Kernel cmdline:** "idle=nomwait intel_idle.max_cstate=0 intel_pstate=disable"

#. Customize your needs.

   .. note:: **[TO BE DEVELOPED]**
      The tool in this step is still under development. Until then,
      you can input settings by editing the target XML file manually.

   a. Copy ``$(BOARD).xml`` to the host develop machine.
   #. Run a UI based configuration tool on the host machine to input the
      desired scenario settings.  The tool will do a sanity check on the
      input based on ``$(BOARD).xml`` and then generate a customized
      scenario-based VM configurations in ``$(SCENARIO).xml``.
   #. In the configuration tool UI, input the launch script parameters for the
      post-launched User VM. The tool will sanity check the input based on
      both ``$(BOARD).xml`` and ``$(SCENARIO).xml`` and then generate a launch
      script-based VM configuration in ``$(LAUNCH_PARAM).xml``.

#. Auto generate code.

   Python tools are used to generate configurations in patch format.
   The patches will be applied to your local ``acrn-hypervisor`` git tree
   automatically.

   a. Generate a patch for the board-related configuration with::

         cd misc/board_config
         python3 board_cfg_gen.py --board $(BOARD).xml

   #. **[TO BE DEVELOPED]** Generate a patch for scenario-based VM
      configuration with::

         cd misc/scenario_config
         python3 scenario_cfg_gen.py --board $(BOARD).xml --scenario

   #. **[TO BE DEVELOPED]** Generate the launch script for the specified
      post-launch User VM with::

         cd misc/launch_config
         python3 launch_cfg_gen.py --board $(BOARD).xml --scenario $(SCENARIO).xml --launch $(LAUNCH_PARAM).xml$

#. Re-build the ACRN hypervisor. Please refer to the
   :ref:`getting-started-building` to re-build ACRN hypervisor on host machine.

#. Deploy VMs and run ACRN hypervisor on target board.

.. figure:: images/offline_tools_workflow.png
   :align: center

   offline tool workflow

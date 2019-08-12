.. _acrn_configuration_tool:

ACRN Configuration Tool
#######################

ACRN Configurations Introduction
********************************
There are three types of configurations in ACRN: Hypervisor configurations,
Board configurations and VM configurations.

Hypervisor configurations
=========================
Hypervisor configurations is used to select working scenario and target board,
configure features and capabilities, set up log and serial port etc. to build
differential hypervisor image for different board.

Hypervisor configurations is configured by ``Kconfig`` mechanism.
The configuration file is located at::

   acrn-hypervisor/hypervisor/arch/x86/configs/Kconfig

A defconfig file which located at::

   acrn-hypervisor/hypervisor/arch/x86/configs/$(BOARD)/$(BOARD).config

will be loaded first as the default ``Kconfig`` for the specified board.

Board configurations
====================
Board configurations is used to store board related configurations that referenced
by ACRN hypervisor, including scenario irrelevant information and scenario relevant
information. Scenario irrelevant information is hardware specific, like ACPI/PCI
BDF info/.etc. Scenario relevant information is board settings in each scenario
like root device selection/kernel cmdline. The board configuration is organized
as ``*.c/*.h`` file and located at::

   acrn-hypervisor/hypervisor/arch/x86/$(BOARD)/

VM configurations
=================
VM configurations includes scenario based VM configurations and launch script
based VM configurations. The former one is used to describe characteristic/attributes
of the VM on each user scenario, another is launch script parameters for device model
to launch post-launched User VM. Scenario based VM configurations is organized
as ``*.c/*.h`` file and located at::

   acrn-hypervisor/hypervisor/scenarios/$(SCENARIO)/

User VM launch script samples are located at::

   acrn-hypervisor/devicemodel/samples/

Configuration Tool Working Flow
*******************************

Kconfig
=======
Hypervisor configurations is based on ``Kconfig`` ``make menuconfig`` mechanism,
user could configure it in menuconfig GUI by using ``make menuconfig``
command to generate the needed ``.config`` file for hypervisor build usage.
Before running ``make menuconfig``, user need to create board specific ``defconfig``
to setup default ``Kconfig`` value for the specified board.

.. figure:: images/sample_of_defconfig.png
   :align: center

   Sample of defconfig file

.. figure:: images/GUI_of_menuconfig.png
   :align: center

   GUI of menuconfig

Please refer to the :ref:`getting-started-hypervisor-configuration` for detailed steps.

Offline configure tool
======================
For Board configurations and VM configurations, an offline configure tool is
designed to configure them. The tool source folder is located at::

   acrn-hypervisor/misc/acrn-config/

Below is offline configure tool working flow:

#. Get board info.

   a. Setup native Linux environment on target board.
   #. Copy ``target`` folder into target file system and then run
      ``sudo python3 board_parser.py $(BOARD)`` command.
   #. A $(BOARD).xml which includes all needed hardware specific information
      will be generated at ``./out/`` folder. (Here $(BOARD) is the specified board name)

       | **Native Linux requirement:**
       | **Release:** Ubuntu 18.04+ or ClearLinux 30210+
       | **Tools:** cpuid, rdmsr, lspci, dmidecode(optional)
       | **Kernel cmdline:** "idle=nomwait intel_idle.max_cstate=0 intel_pstate=disable"

#. Customize your needs.

   .. note:: **[TO BE DEVELOPED]**
      The tool in this step is still under development, before its readiness user
      could input the setting by editing the target XML file manually.

   a. Copy ``$(BOARD).xml`` to host develop machine;
   #. Run a UI based configure tool on host machine to input the expected scenario settings,
      the tool will do sanity check on the input based on ``$(BOARD).xml`` and then generate
      customized scenario based VM configurations in ``$(SCENARIO).xml``;
   #. In the UI of configure tool, continue to input launch script parameter for
      post-launched User VM. The tool will check the input based on both ``$(BOARD).xml``
      and ``$(SCENARIO).xml`` and then generate launch script based VM configurations in
      ``$(LAUNCH_PARAM).xml``;

#. Auto generate code.
   There are three python tools will be used to generate configurations in patch format.
   The patches will be applied to ``acrn-hypervisor`` git tree automatically.

   a. Run ``python3 board_cfg_gen.py --board $(BOARD).xml`` under ``misc/board_config``
      folder, it will generate patch for board related configurations;
   #. **[TO BE DEVELOPED]** Run ``python3 scenario_cfg_gen.py --board $(BOARD).xml --scenario
      $(SCENARIO).xml`` under ``misc/scenario_config`` folder, it will generate patch
      for scenario based VM configurations;
   #. **[TO BE DEVELOPED]** Run ``python3 launch_cfg_gen.py --board $(BOARD).xml
      --scenario $(SCENARIO).xml --launch $(LAUNCH_PARAM).xml$`` under ``misc/launch_config``
      folder, it will generate launch script for the specified post-launch User VM;

#. Re-build ACRN hypervisor. Please refer to the :ref:`getting-started-building`
   to re-build ACRN hypervisor on host machine;

#. Deploy VMs and run ACRN hypervisor on target board.

.. figure:: images/offline_tools_workflow.png
   :align: center

   offline tool working flow
.. _acrn_configuration_tool:

Introduction to ACRN Configuration
##################################

ACRN configuration is designed for System Integrators / Tier 1s to customize
ACRN to meet their own needs. It allows users to adapt ACRN to target boards as
well as configure hypervisor capabilities and provision VMs.

ACRN configuration consists of the following key components.

 - Configuration data which are saved as XML files.
 - A configuration toolset that helps users to generate and edit configuration
   data. The toolset includes:

   - A **board inspector** which collects board-specific information on target
     machines.
   - A **configuration editor** which edits configuration data via web-based UI.

The following sections introduce the concepts and tools of ACRN configuration
from the aspects below.

 - :ref:`acrn_config_types` introduces the objectives and main contents of
   different types of configuration data.
 - :ref:`acrn_config_workflow` overviews the steps to customize ACRN
   configuration using the configuration toolset.
 - :ref:`acrn_config_data` explains the location and format of configuration
   data which are saved as XML files.
 - :ref:`acrn_config_tool_ui` gives detailed instructions on using the
   configuration editor.

.. _acrn_config_types:

Types of Configurations
***********************

ACRN includes three types of configurations: board, scenario and launch. The
following sections briefly describe the objectives and main contents of each
type.

Board Configuration
===================

The board configuration stores hardware-specific information extracted on the
target platform. It describes the capacity of hardware resources (such as
processors and memory), platform power states, available devices and BIOS
versions. This information is used by ACRN configuration tool to check feature
availability and allocate resources among VMs, as well as by ACRN hypervisor to
initialize and manage the platform at runtime.

The board configuration is scenario-neutral by nature. Thus, multiple scenario
configurations can be based on the same board configuration.

Scenario Configuration
======================

The scenario configuration defines a working scenario by configuring hypervisor
capabilities and defining VM attributes and resources. You can specify the
following in scenario configuration.

 - Hypervisor capabilities

   - Availability and settings of hypervisor features, such as debugging
     facilities, scheduling algorithm, ivshmem and security features.
   - Hardware management capacity of the hypervisor, such as maximum PCI devices
     and maximum interrupt lines supported.
   - Memory consumption of the hypervisor, such as the entry point and stack
     size.

 - VM attributes and resources

   - VM attributes, such as VM names.
   - Maximum number of VMs supported.
   - Resources allocated to each VM, such as number of vCPUs, amount of guest
     memory and pass-through devices.
   - Guest OS settings, such as boot protocol and guest kernel parameters.
   - Settings of virtual devices, such as virtual UARTs.

For pre-launched VMs, the VM attributes and resources are exactly the amount of
resource allocated to them. For post-launched VMs, the number of vCPUs define
the upper limit the Service VM can allocate to them and settings of virtual
devices still apply. Other resources are under the control of the Service VM and
can be dynamically allocated to post-launched VMs.

The scenario configuration is used by ACRN configuration tool to reserve
sufficient memory for the hypervisor to manage the VMs at build time, as well as
by ACRN hypervisor to initialize its capabilities and set up the VMs at runtime.

Launch Configuration
====================

The launch configuration defines the attributes and resources of a
post-launched VM. The main contents are similar to the VM attributes and
resources in scenario configuration. It is used to generate shell scripts that
invoke ``acrn-dm`` to create post-launched VMs. Unlike board and scenario
configurations which are used at build time or by ACRN hypervisor, launch
configuration are used dynamically in the Service VM.

.. _acrn_config_workflow:

Using ACRN Configuration Toolset
********************************

ACRN configuration toolset is provided to create and edit configuration
data. The toolset can be found in ``misc/config_tools``.

Here is the workflow to customize ACRN configurations using the configuration
toolset.

#. Get the board info.

   a. Set up a native Linux environment on the target board. Make sure the
      following tools are installed and the kernel boots with the following
      command line options.

      | **Native Linux requirement:**
      | **Release:** Ubuntu 18.04+
      | **Tools:** cpuid, rdmsr, lspci, dmidecode (optional)
      | **Kernel cmdline:** "idle=nomwait intel_idle.max_cstate=0 intel_pstate=disable"

   #. Copy the ``target`` directory into the target file system and then run the
      ``sudo python3 board_parser.py $(BOARD)`` command.
   #. A ``$(BOARD).xml`` that includes all needed hardware-specific information
      is generated in the ``./out/`` directory. Here, ``$(BOARD)`` is the
      specified board name.

#. Customize your needs.

   a. Copy ``$(BOARD).xml`` to the host development machine.
   #. Run the ACRN configuration editor (available at
      ``misc/config_tools/config_app/app.py``) on the host machine and import
      the ``$(BOARD).xml``. Select your working scenario under **Scenario Setting**
      and input the desired scenario settings. The tool will do a sanity check
      on the input based on the ``$(BOARD).xml``. The customized settings can be
      exported to your own ``$(SCENARIO).xml``. If you have a customized scenario
      XML file, you can also import it to the editor for modification.
   #. In ACRN configuration editor, input the launch script parameters for the
      post-launched User VM under **Launch Setting**. The editor will sanity check
      the input based on both the ``$(BOARD).xml`` and ``$(SCENARIO).xml`` and then
      export settings to your ``$(LAUNCH).xml``.

   .. note:: Refer to :ref:`acrn_config_tool_ui` for more details on
      the configuration editor.

#. Build with your XML files. Refer to :ref:`getting-started-building` to build
   the ACRN hypervisor with your XML files on the host machine.

#. Deploy VMs and run ACRN hypervisor on the target board.

.. figure:: images/offline_tools_workflow.png
   :align: center

   Configuration Workflow

.. _acrn_config_data:

ACRN Configuration Data
***********************

ACRN configuration data are saved in three XML files: ``board``, ``scenario``,
and ``launch`` XML. The ``board`` XML contains board configuration and is
generated by the board inspector on the target machine. The ``scenario`` and
``launch`` XMLs, containing scenario and launch configurations respectively, can
be customized by using the configuration editor. End users can load their own
configurations by importing customized XMLs or by saving the configurations by
exporting XMLs.

The predefined XMLs provided by ACRN are located in the ``misc/config_tools/data/``
directory of the ``acrn-hypervisor`` repo.

Board XML Format
================

The board XML has an ``acrn-config`` root element and a ``board`` attribute:

.. code-block:: xml

   <acrn-config board="BOARD">

As an input to the configuration editor and the build system, board XMLs are not
intended for end users to modify.

Scenario XML Format
===================

The scenario XML has an ``acrn-config`` root element as well as ``board`` and
``scenario`` attributes:

.. code-block:: xml

   <acrn-config board="BOARD" scenario="SCENARIO">

See :ref:`scenario-config-options` for a full explanation of available scenario
XML elements. Users are recommended to tweak the configuration data by using
ACRN configuration editor.


Launch XML Format
=================

The launch XML has an ``acrn-config`` root element as well as ``board``,
``scenario`` and ``uos_launcher`` attributes:

.. code-block:: xml

   <acrn-config board="BOARD" scenario="SCENARIO" uos_launcher="UOS_NUMBER">

Attributes of the ``uos_launcher`` specify the number of User VMs that the
current scenario has:

``uos``:
  Specify the User VM with its relative ID to Service VM by the ``id`` attribute.

``uos_type``:
  Specify the User VM type, such as ``CLEARLINUX``, ``ANDROID``, ``ALIOS``,
  ``PREEMPT-RT LINUX``, ``GENERIC LINUX``, ``WINDOWS``, ``YOCTO``, ``UBUNTU``,
  ``ZEPHYR`` or ``VXWORKS``.

``rtos_type``:
  Specify the User VM Real-time capability: Soft RT, Hard RT, or none of them.

``mem_size``:
  Specify the User VM memory size in megabytes.

``gvt_args``:
  GVT arguments for the VM. Set it to ``gvtd`` for GVT-d, otherwise it's
  for GVT-g arguments.  The GVT-g input format: ``low_gm_size high_gm_size fence_sz``,
  The recommendation is ``64 448 8``.  Leave it blank to disable the GVT.

``vbootloader``:
  Virtual bootloader type; currently only supports OVMF.

``vuart0``:
  Specify whether the device model emulates the vUART0(vCOM1); refer to
  :ref:`vuart_config` for details.  If set to ``Enable``, the vUART0 is
  emulated by the device model; if set to ``Disable``, the vUART0 is
  emulated by the hypervisor if it is configured in the scenario XML.

``poweroff_channel``:
  Specify whether the User VM power off channel is through the IOC,
  power button, or vUART.

``usb_xhci``:
  USB xHCI mediator configuration. Input format:
  ``bus#-port#[:bus#-port#: ...]``, e.g.: ``1-2:2-4``.
  Refer to :ref:`usb_virtualization` for details.

``shm_regions``:
  List of shared memory regions for inter-VM communication.

``shm_region`` (a child node of ``shm_regions``):
  configure the shared memory regions for current VM, input format:
  ``hv:/<;shm name>;, <;shm size in MB>;``. Refer to :ref:`ivshmem-hld` for details.

``passthrough_devices``:
  Select the passthrough device from the lspci list. Currently we support:
  ``usb_xdci``, ``audio``, ``audio_codec``, ``ipu``, ``ipu_i2c``,
  ``cse``, ``wifi``, ``bluetooth``, ``sd_card``,
  ``ethernet``, ``sata``, and ``nvme``.

``network`` (a child node of ``virtio_devices``):
  The virtio network device setting.
  Input format: ``tap_name,[vhost],[mac=XX:XX:XX:XX:XX:XX]``.

``block`` (a child node of ``virtio_devices``):
  The virtio block device setting.
  Input format: ``[blk partition:][img path]`` e.g.: ``/dev/sda3:./a/b.img``.

``console`` (a child node of ``virtio_devices``):
  The virtio console device setting.
  Input format:
  ``[@]stdio|tty|pty|sock:portname[=portpath][,[@]stdio|tty|pty:portname[=portpath]]``.

.. note::

   The ``configurable`` and ``readonly`` attributes are used to mark
   whether the item is configurable for users. When ``configurable="0"``
   and ``readonly="true"``, the item is not configurable from the web
   interface. When ``configurable="0"``, the item does not appear on the
   interface.

.. _acrn_config_tool_ui:

Use the ACRN Configuration Editor
*********************************

The ACRN configuration editor provides a web-based user interface for the following:

- reads board info
- configures and validates scenario and launch configurationss
- generates launch scripts for the specified post-launched User VMs.
- dynamically creates a new scenario configuration and adds or deletes VM
  settings in it
- dynamically creates a new launch configuration and adds or deletes User VM
  settings in it

Prerequisites
=============

.. _get acrn repo guide:
   https://projectacrn.github.io/latest/getting-started/building-from-source.html#get-the-acrn-hypervisor-source-code

- Clone acrn-hypervisor:

  .. code-block:: none

     $ git clone https://github.com/projectacrn/acrn-hypervisor

- Install ACRN configuration editor dependencies:

  .. code-block:: none

     $ cd ~/acrn-hypervisor/misc/acrn-config/config_app
     $ sudo pip3 install -r requirements


Instructions
============

#. Launch the ACRN configuration editor:

   .. code-block:: none

      $ python3 app.py

#. Open a browser and navigate to the website
   `<http://127.0.0.1:5001/>`_ automatically, or you may need to visit this
   website manually. Make sure you can connect to open network from browser
   because the editor needs to download some JavaScript files.

   .. note:: The ACRN configuration editor is supported on Chrome, Firefox,
      and Microsoft Edge. Do not use Internet Explorer.

   The website is shown below:

   .. figure:: images/config_app_main_menu.png
      :align: center
      :name: ACRN config tool main menu

#. Set the board info:

   a. Click **Import Board info**.

      .. figure:: images/click_import_board_info_button.png
         :align: center

   #. Upload the board XML you have generated from the ACRN board inspector.

   #. After board XML is uploaded, you will see the board name from the
      Board info list. Select the board name to be configured.

      .. figure:: images/select_board_info.png
         :align: center

#. Load or create the scenario configuration by selecting among the following:

   - Choose a scenario from the **Scenario Setting** menu that lists all
     user-defined scenarios for the board you selected in the previous step.

   - Click the **Create a new scenario** from the **Scenario Setting** menu to
     dynamically create a new scenario configuration for the current board.

   - Click the **Load a default scenario** from the **Scenario Setting** menu,
     and then select one default scenario configuration to load a predefined
     scenario XML for the current board.

   The default scenario XMLs are located at
   ``misc/config_tools/data/[board]/``. You can edit the scenario name when
   creating or loading a scenario. If the current scenario name is duplicated
   with an existing scenario setting name, rename the current scenario name or
   overwrite the existing one after the confirmation message.

   .. figure:: images/choose_scenario.png
      :align: center

   Note that you can also use a customized scenario XML by clicking **Import
   XML**. The configuration editor automatically directs to the new scenario
   XML once the import is complete.

#. The configurable items display after one scenario is created, loaded,
   or selected. Following is an industry scenario:

   .. figure:: images/configure_scenario.png
      :align: center

   - You can edit these items directly in the text boxes, or you can choose
     single or even multiple items from the drop-down list.

   - Read-only items are marked as gray.

   - Hover the mouse cursor over the item to display the description.

#. Dynamically add or delete VMs:

   - Click **Add a VM below** in one VM setting, and then select one VM type
     to add a new VM under the current VM.

   - Click **Remove this VM** in one VM setting to remove the current VM for
     the scenario setting.

   When one VM is added or removed in the scenario, the configuration editor
   reassigns the VM IDs for the remaining VMs by the order of Pre-launched VMs,
   Service VMs, and Post-launched VMs.

   .. figure:: images/configure_vm_add.png
      :align: center

#. Click **Export XML** to save the scenario XML; you can rename it in the
   pop-up model.

   .. note::
      All customized scenario XMLs will be in user-defined groups, which are
      located in ``misc/config_tools/data/[board]/user_defined/``.

   Before saving the scenario XML, the configuration editor validates the
   configurable items. If errors exist, the configuration editor lists all
   incorrectly configured items and shows the errors as below:

   .. figure:: images/err_acrn_configuration.png
      :align: center

   After the scenario is saved, the page automatically directs to the saved
   scenario XMLs. Delete the configured scenario by clicking **Export XML** -> **Remove**.

The **Launch Setting** is quite similar to the **Scenario Setting**:

#. Upload board XML or select one board as the current board.

#. Load or create one launch configuration by selecting among the following:

   - Click **Create a new launch script** from the **Launch Setting** menu.

   - Click **Load a default launch script** from the **Launch Setting** menu.

   - Select one launch XML from the menu.

   - Import a local launch XML by clicking **Import XML**.

#. Select one scenario for the current launch configuration from the **Select
   Scenario** drop-down box.

#. Configure the items for the current launch configuration.

#. To dynamically add or remove User VM (UOS) launch scripts:

   - Add a UOS launch script by clicking **Configure an UOS below** for the
     current launch configuration.

   - Remove a UOS launch script by clicking **Remove this VM** for the
     current launch configuration.

#. Save the current launch configuration to the user-defined XML files by
   clicking **Export XML**. The configuration editor validates the current
   configuration and lists all incorrectly configured items.

#. Click **Generate Launch Script** to save the current launch configuration and
   then generate the launch script.

   .. figure:: images/generate_launch_script.png
      :align: center

.. _board_inspector_tool:

Board Inspector Tool
####################

This guide describes all features and uses of the tool.

About the Board Inspector Tool
******************************

The Board Inspector tool ``board_inspector.py`` enables you to generate a board
configuration file on the target system. The board configuration file stores
hardware-specific information extracted from the target platform and is used to
customize your :ref:`ACRN configuration <acrn_configuration_tool>`.

Generate a Board Configuration File
***********************************

.. important::

   Whenever you change the configuration of the board, such as BIOS settings,
   additional memory, or PCI devices, you must generate a new board
   configuration file.

The following steps describe all options in the Board Inspector for generating
a board configuration file.

#. Make sure the target system is set up and ready to run the Board Inspector,
   according to :ref:`gsg-board-setup` in the Getting Started Guide.

#. Run the Board Inspector tool (``board_inspector.py``) to generate the board
   configuration file. This example assumes the tool is in the
   ``~/acrn-work/`` directory and ``my_board`` is the desired file
   name. Feel free to modify the commands as needed.

   .. code-block:: bash

      cd ~/acrn-work/board_inspector/
      sudo python3 board_inspector.py my_board

   Upon success, the tool displays a message similar to this example:

   .. code-block:: console

      my_board.xml saved successfully!

#. Confirm that the board configuration file ``my_board.xml`` was generated in
   the current directory.

.. _board_inspector_cl:

Command-Line Options
********************

You can configure the Board Inspector via command-line options. Running the
Board Inspector with the ``-h`` option yields the following usage message:

.. code-block::

   usage: board_inspector.py [-h] [--out OUT] [--basic] [--loglevel LOGLEVEL]
                [--check-device-status] board_name

   positional arguments:
     board_name            the name of the board that runs the ACRN hypervisor

   optional arguments:
     -h, --help            show this help message and exit
     --out OUT             the name of board info file
     --basic               do not extract advanced information such as ACPI namespace
     --loglevel LOGLEVEL   choose log level, e.g. info, warning or error
     --check-device-status

                           filter out devices whose _STA object evaluates to 0

Details about certain arguments:

.. list-table::
   :widths: 33 77
   :header-rows: 1

   * - Argument
     - Details

   * - ``board_name``
     - Required. The board name is used as the file name of the board
       configuration file and is placed inside the file for other tools to read.

   * - ``--out``
     - Optional. Specify a file path where the board configuration file will be
       saved (example: ``~/acrn_work``). If only a filename is provided in this
       option, the Board Inspector will generate the file in the current
       directory.

   * - ``--basic``
     - Optional. By default, the Board Inspector parses the ACPI namespace when
       generating board configuration files. This option provides a way to
       disable ACPI namespace parsing in case the parsing blocks the generation
       of board configuration files.

   * - ``--loglevel``
     - Optional. Choose log level, e.g., info, warning or error.
       (Default is warning.)

   * - ``--check-device-status``
     - Optional. On some boards, the device status (reported by the _STA
       object) returns 0 while the device object is still useful for
       pass-through devices. By default, the Board Inspector includes the
       devices in the board configuration file. This option filters out the
       devices, so that they cannot be used.

.. _getting-started-building:

Build ACRN From Source
######################

Following a general embedded-system programming model, the ACRN
hypervisor is designed to be customized at build time per hardware
platform and per usage scenario, rather than one binary for all
scenarios.

The hypervisor binary is generated based on configuration settings in XML
files. Instructions about customizing these settings can be found in
:ref:`getting-started-hypervisor-configuration`.

One binary for all platforms and all usage scenarios is not
supported. Dynamic configuration parsing is not used in
the ACRN hypervisor for these reasons:

- **Maintain functional safety requirements.** Implementing dynamic parsing
  introduces dynamic objects, which violate functional safety requirements.

- **Reduce complexity.** ACRN is a lightweight reference hypervisor, built for
  embedded IoT. As new platforms for embedded systems are rapidly introduced,
  support for one binary could require more and more complexity in the
  hypervisor, which is something we strive to avoid.

- **Maintain small footprint.** Implementing dynamic parsing introduces
  hundreds or thousands of lines of code. Avoiding dynamic parsing
  helps keep the hypervisor's Lines of Code (LOC) in a desirable range (less
  than 40K).

- **Improve boot time.** Dynamic parsing at runtime increases the boot
  time. Using a build-time configuration and not dynamic parsing
  helps improve the boot time of the hypervisor.


Build the ACRN hypervisor, device model, and tools from source by following
these steps.

.. contents::
   :local:
   :depth: 1

.. _install-build-tools-dependencies:

.. rst-class:: numbered-step

Install Build Tools and Dependencies
************************************

ACRN development is supported on popular Linux distributions, each with their
own way to install development tools. This user guide covers the steps to
configure and build ACRN natively on **Ubuntu 18.04 or newer**.

The following commands install the necessary tools for configuring and building
ACRN.

  .. code-block:: none

     sudo apt install gcc \
          git \
          make \
          libssl-dev \
          libpciaccess-dev \
          uuid-dev \
          libsystemd-dev \
          libevent-dev \
          libxml2-dev \
          libxml2-utils \
          libusb-1.0-0-dev \
          python3 \
          python3-pip \
          libblkid-dev \
          e2fslibs-dev \
          pkg-config \
          libnuma-dev \
          liblz4-tool \
          flex \
          bison \
          xsltproc \
          clang-format

     sudo pip3 install lxml xmlschema

     wget https://acpica.org/sites/acpica/files/acpica-unix-20210105.tar.gz
     tar zxvf acpica-unix-20210105.tar.gz
     cd acpica-unix-20210105
     make clean && make iasl
     sudo cp ./generate/unix/bin/iasl /usr/sbin/

.. rst-class:: numbered-step

Get the ACRN Hypervisor Source Code
***********************************

The `ACRN hypervisor <https://github.com/projectacrn/acrn-hypervisor/>`_
repository contains four main components:

1. The ACRN hypervisor code is in the ``hypervisor`` directory.
#. The ACRN device model code is in the ``devicemodel`` directory.
#. The ACRN debug tools source code is in the ``misc/debug_tools`` directory.
#. The ACRN online services source code is in the ``misc/services`` directory.

Enter the following to get the ACRN hypervisor source code:

.. code-block:: none

   git clone https://github.com/projectacrn/acrn-hypervisor


.. _build-with-acrn-scenario:

.. rst-class:: numbered-step

Build With the ACRN Scenario
****************************

Currently, the ACRN hypervisor defines these typical usage scenarios:

SDC:
   The SDC (Software Defined Cockpit) scenario defines a simple
   automotive use case that includes one pre-launched Service VM and one
   post-launched User VM.

LOGICAL_PARTITION:
    This scenario defines two pre-launched VMs.

INDUSTRY:
   This scenario is an example for industrial usage with up to eight VMs:
   one pre-launched Service VM, five post-launched Standard VMs (for Human
   interaction etc.), one post-launched RT VMs (for real-time control),
   and one Kata Container VM.

HYBRID:
   This scenario defines a hybrid use case with three VMs: one
   pre-launched Safety VM, one pre-launched Service VM, and one post-launched
   Standard VM.

HYBRID_RT:
   This scenario defines a hybrid use case with three VMs: one
   pre-launched RTVM, one pre-launched Service VM, and one post-launched
   Standard VM.

XML configuration files for these scenarios on supported boards are available
under the ``misc/config_tools/data`` directory.

Assuming that you are at the top level of the ``acrn-hypervisor`` directory, perform
the following to build the hypervisor, device model, and tools:

.. note::
   The debug version is built by default. To build a release version,
   build with ``RELEASE=y`` explicitly, regardless of whether a previous
   build exists.

* Build the debug version of ``INDUSTRY`` scenario on the ``nuc7i7dnb``:

  .. code-block:: none

     make BOARD=nuc7i7dnb SCENARIO=industry

* Build the release version of ``HYBRID`` scenario on the ``whl-ipc-i5``:

  .. code-block:: none

     make BOARD=whl-ipc-i5 SCENARIO=hybrid RELEASE=y

* Build the release version of ``HYBRID_RT`` scenario on the ``whl-ipc-i7``
  (hypervisor only):

  .. code-block:: none

     make BOARD=whl-ipc-i7 SCENARIO=hybrid_rt RELEASE=y hypervisor

* Build the release version of the device model and tools:

  .. code-block:: none

     make RELEASE=y devicemodel tools

You can also build ACRN with your customized scenario:

* Build with your own scenario configuration on the ``nuc11tnbi5``, assuming the
  scenario is defined in ``/path/to/scenario.xml``:

  .. code-block:: none

     make BOARD=nuc11tnbi5 SCENARIO=/path/to/scenario.xml

* Build with your own board and scenario configuration, assuming the board and
  scenario XML files are ``/path/to/board.xml`` and ``/path/to/scenario.xml``:

  .. code-block:: none

     make BOARD=/path/to/board.xml SCENARIO=/path/to/scenario.xml

.. note::
   ACRN uses XML files to summarize board characteristics and scenario
   settings. The ``BOARD`` and ``SCENARIO`` variables accept board/scenario
   names as well as paths to XML files. When board/scenario names are given, the
   build system searches for XML files with the same names under
   ``misc/config_tools/data/``. When paths (absolute or relative) to the XML
   files are given, the build system uses the files pointed at. If relative
   paths are used, they are considered relative to the current working
   directory.

See the :ref:`hardware` document for information about platform needs for each
scenario. For more instructions to customize scenarios, see
:ref:`getting-started-hypervisor-configuration` and
:ref:`acrn_configuration_tool`.

The build results are found in the ``build`` directory. You can specify
a different build directory by setting the ``O`` ``make`` parameter,
for example: ``make O=build-nuc``.

To query the board, scenario, and build type of an existing build, the
``hvshowconfig`` target will help.

  .. code-block:: none

    $ make BOARD=tgl-rvp SCENARIO=hybrid_rt hypervisor
    ...
    $ make hvshowconfig
    Build directory: /path/to/acrn-hypervisor/build/hypervisor
    This build directory is configured with the settings below.
    - BOARD = tgl-rvp
    - SCENARIO = hybrid_rt
    - RELEASE = n

.. _getting-started-hypervisor-configuration:

.. rst-class:: numbered-step

Modify the Hypervisor Configuration
***********************************

The ACRN hypervisor is built with scenario encoded in an XML file (referred to
as the scenario XML hereinafter). The scenario XML of a build can be found at
``<build>/hypervisor/.scenario.xml``, where ``<build>`` is the name of the build
directory. You can make further changes to this file to adjust to your specific
requirements. Another ``make`` will rebuild the hypervisor using the updated
scenario XML.

The following commands show how to customize manually the scenario XML based on
the predefined ``INDUSTRY`` scenario for ``nuc7i7dnb`` and rebuild the
hypervisor. The ``hvdefconfig`` target generates the configuration files without
building the hypervisor, allowing users to tweak the configurations.

.. code-block:: none

   make BOARD=nuc7i7dnb SCENARIO=industry hvdefconfig
   vim build/hypervisor/.scenario.xml
   #(Modify the XML file per your needs)
   make

.. note::
   A hypervisor build remembers the board and scenario previously
   configured. Thus, there is no need to duplicate BOARD and SCENARIO in the
   second ``make`` above.

While the scenario XML files can be changed manually, we recommend you use the
ACRN web-based configuration app that provides valid options and descriptions
of the configuration entries. Refer to :ref:`acrn_config_tool_ui` for more
instructions.

Descriptions of each configuration entry in scenario XML files are also
available at :ref:`scenario-config-options`.

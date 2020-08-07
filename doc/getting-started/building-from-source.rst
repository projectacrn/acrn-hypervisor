.. _getting-started-building:

Build ACRN from Source
######################

Introduction
************

Following a general embedded-system programming model, the ACRN
hypervisor is designed to be customized at build time per hardware
platform and per usage scenario, rather than one binary for all
scenarios.

The hypervisor binary is generated based on Kconfig configuration
settings. Instructions about these settings can be found in
:ref:`getting-started-hypervisor-configuration`.

One binary for all platforms and all usage scenarios is currently not
supported, primarily because dynamic configuration parsing is restricted in
the ACRN hypervisor for the following reasons:

- **Maintain functional safety requirements.** Implementing dynamic parsing
  introduces dynamic objects, which violates functional safety requirements.

- **Reduce complexity.** ACRN is a lightweight reference hypervisor, built for
  embedded IoT. As new platforms for embedded systems are rapidly introduced,
  support for one binary could require more and more complexity in the
  hypervisor, which is something we strive to avoid.

- **Maintain small footprint.** Implementing dynamic parsing introduces
  hundreds or thousands of lines of code. Avoiding dynamic parsing
  helps keep the hypervisor's Lines of Code (LOC) in a desirable range (less
  than 40K).

- **Improve boot up time.** Dynamic parsing at runtime increases the boot
  up time. Using a build-time configuration and not dynamic parsing
  helps improve the boot up time of the hypervisor.


Build the ACRN hypervisor, device model, and tools from source by following
these steps.

.. _install-build-tools-dependencies:

.. rst-class:: numbered-step

Install build tools and dependencies
************************************

ACRN development is supported on popular Linux distributions, each with
their own way to install development tools. This user guide covers the
different steps to configure and build ACRN natively on your
distribution. Refer to the :ref:`building-acrn-in-docker` user guide for
instructions on how to build ACRN using a container.

.. note::
   ACRN uses ``menuconfig``, a python3 text-based user interface (TUI)
   for configuring hypervisor options and using python's ``kconfiglib``
   library.

Install the necessary tools for the following systems:

* Clear Linux OS development system:

  .. code-block:: none

     $ sudo swupd bundle-add os-clr-on-clr os-core-dev python3-basic
     $ pip3 install --user kconfiglib

* Ubuntu development system:

  .. code-block:: none

     $ sudo apt install gcc \
          git \
          make \
          gnu-efi \
          libssl-dev \
          libpciaccess-dev \
          uuid-dev \
          libsystemd-dev \
          libevent-dev \
          libxml2-dev \
          libusb-1.0-0-dev \
          python3 \
          python3-pip \
          libblkid-dev \
          e2fslibs-dev \
          pkg-config \
          libnuma-dev
     $ sudo pip3 install kconfiglib

  .. note::
     ACRN requires ``gcc`` version 7.3.* (or higher) and ``binutils`` version
     2.27 (or higher). Check your development environment to ensure you have
     appropriate versions of these packages by using the commands: ``gcc -v``
     and ``ld -v``.

.. rst-class:: numbered-step

Get the ACRN hypervisor source code
***********************************

The `acrn-hypervisor <https://github.com/projectacrn/acrn-hypervisor/>`_
repository contains four main components:

1. The ACRN hypervisor code, located in the ``hypervisor`` directory.
#. The EFI stub code, located in the ``misc/efi-stub`` directory.
#. The ACRN device model code, located in the ``devicemodel`` directory.
#. The ACRN tools source code, located in the ``misc/tools`` directory.

Enter the following to get the acrn-hypervisor source code:

.. code-block:: none

   $ git clone https://github.com/projectacrn/acrn-hypervisor


.. _build-with-acrn-scenario:

.. rst-class:: numbered-step

Build with the ACRN scenario
****************************

Currently, the ACRN hypervisor defines these typical usage scenarios:

SDC:
   The SDC (Software Defined Cockpit) scenario defines a simple
   automotive use case that includes one pre-launched Service VM and one
   post-launched User VM.

LOGICAL_PARTITION:
    This scenario defines two pre-launched VMs.

INDUSTRY:
   This is a typical scenario for industrial usage with up to eight VMs:
   one pre-launched Service VM, five post-launched Standard VMs (for Human
   interaction etc.), one post-launched RT VMs (for real-time control),
   and one Kata container VM.

HYBRID:
   This scenario defines a hybrid use case with three VMs: one
   pre-launched Safety VM, one pre-launched Service VM, and one post-launched
   Standard VM.

HYBRID_RT:
   This scenario defines a hybrid use case with three VMs: one
   pre-launched RTVM, one pre-launched Service VM, and one post-launched
   Standard VM.

Assuming that you are at the top level of the acrn-hypervisor directory, perform the following:

.. note::
   The release version is built by default, ``RELEASE=0`` builds the debug version.

* Build the ``INDUSTRY`` scenario on the ``nuc7i7dnb``:

  .. code-block:: none

     $ make all BOARD=nuc7i7dnb SCENARIO=industry RELEASE=0

* Build the ``HYBRID`` scenario on the ``whl-ipc-i5``:

  .. code-block:: none

     $ make all BOARD=whl-ipc-i5 SCENARIO=hybrid RELEASE=0

* Build the ``HYBRID_RT`` scenario on the ``whl-ipc-i7``:

  .. code-block:: none

     $ make all BOARD=whl-ipc-i7 SCENARIO=hybrid_rt RELEASE=0

* Build the ``SDC`` scenario on the ``nuc6cayh``:

  .. code-block:: none

    $ make all BOARD_FILE=$PWD/misc/vm_configs/xmls/board-xmls/nuc6cayh.xml \
    SCENARIO_FILE=$PWD/misc/vm_configs/xmls/config-xmls/nuc6cayh/sdc.xml


See the :ref:`hardware` document for information about platform needs
for each scenario.

.. _getting-started-hypervisor-configuration:

.. rst-class:: numbered-step

Build the hypervisor configuration
**********************************

Modify the hypervisor configuration
===================================

The ACRN hypervisor leverages Kconfig to manage configurations; it is
powered by ``Kconfiglib``. A default configuration is generated based on the
board you have selected via the ``BOARD=`` command line parameter. You can
make further changes to that default configuration to adjust to your specific
requirements.

To generate hypervisor configurations, you must build the hypervisor
individually. The following steps generate a default but complete
configuration, based on the platform selected, assuming that you are at the
top level of the acrn-hypervisor directory. The configuration file, named
``.config``, can be found under the target folder of your build.

.. code-block:: none

   $ cd hypervisor
   $ make defconfig BOARD=nuc7i7dnb SCENARIO=industry

The BOARD specified is used to select a ``defconfig`` under
``misc/vm_configs/scenarios/``. The other command line-based options (e.g.
``RELEASE``) take no effect when generating a defconfig.

To modify the hypervisor configurations, you can either edit ``.config``
manually, or you can invoke a TUI-based menuconfig (powered by kconfiglib) by
executing ``make menuconfig``. As an example, the following commands
(assuming that you are at the top level of the acrn-hypervisor directory)
generate a default configuration file for UEFI, allowing you to modify some
configurations and build the hypervisor using the updated ``.config``:

.. code-block:: none

   # Modify the configurations per your needs
   $ cd ../         # Enter top-level folder of acrn-hypervisor source
   $ make menuconfig -C hypervisor
   # modify your own "ACRN Scenario" and "Target board" that want to build
   # in pop up menu

Note that ``menuconfig`` is python3 only.

Refer to the help on menuconfig for a detailed guide on the interface:

.. code-block:: none

   $ pydoc3 menuconfig

.. rst-class:: numbered-step

Build the hypervisor, device model, and tools
*********************************************

Now you can build all these components at once as follows:

.. code-block:: none

   $ make FIRMWARE=uefi       # Build the UEFI hypervisor with the new .config

The build results are found in the ``build`` directory. You can specify
a different Output folder by setting the ``O`` ``make`` parameter,
for example: ``make O=build-nuc BOARD=nuc7i7dnb``.

If you only need the hypervisor, use this command:

.. code-block:: none

   $ make clean                       # Remove files previously built
   $ make -C hypervisor
   $ make -C misc/efi-stub HV_OBJDIR=$PWD/hypervisor/build EFI_OBJDIR=$PWD/hypervisor/build

The ``acrn.efi`` will be generated in the ``./hypervisor/build/acrn.efi`` directory hypervisor.

As mentioned in :ref:`ACRN Configuration Tool <vm_config_workflow>`, the
Board configuration and VM configuration can be imported from XML files.
If you want to build the hypervisor with XML configuration files,
specify the file location as follows (assuming you're at the top level
of the acrn-hypervisor directory):

.. code-block:: none

   $ make BOARD_FILE=$PWD/misc/vm_configs/xmls/board-xmls/nuc7i7dnb.xml \
   SCENARIO_FILE=$PWD/misc/vm_configs/xmls/config-xmls/nuc7i7dnb/industry.xml FIRMWARE=uefi TARGET_DIR=xxx


.. note::
   The ``BOARD`` and ``SCENARIO`` parameters are not needed because the
   information is retrieved from the corresponding ``BOARD_FILE`` and
   ``SCENARIO_FILE`` XML configuration files.  The ``TARGET_DIR`` parameter
   specifies what directory is used to  store configuration files imported
   from XML files. If the ``TARGET_DIR`` is not specified, the original
   configuration files of acrn-hypervisor would be overridden.

   In the 2.1 release, there is a known issue (:acrn-issue:`5157`) that
   ``TARGET_DIR=xxx`` does not work.

Follow the same instructions to boot and test the images you created from your build.

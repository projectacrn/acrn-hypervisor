.. _getting-started-building:

Build ACRN from Source
######################

Introduction
************

Following a general embedded system programming model, the ACRN
hypervisor is designed to be customized at build-time per hardware
platform and per usage scenario, rather than one binary for all
scenarios.

The hypervisor binary is generated based on Kconfig configuration
settings.  Instruction about these settings can be found in
:ref:`getting-started-hypervisor-configuration`.

.. note::
   A generic configuration named ``hypervisor/arch/x86/configs/generic.config``
   is provided to help developers try out ACRN more easily. This configuration
   will likely work for most x86-based platforms, supported with limited features.
   This configuration can be enabled by specifying ``BOARD=generic`` in
   the make command line.


A primary reason one binary for all platforms and all usage scenarios is
not supported is because dynamic configuration parsing is restricted in
ACRN hypervisor, for the following considerations:

* **Meeting functional safety requirements** Absence of dynamic objects is
  required in functional safety standards. Implementation of dynamic parsing
  would introduce dynamic objects. Avoiding use of dynamic
  parsing would help the ACRN hypervisor meet functional safety requirements.

* **Reduce complexity** ACRN is a lightweight reference hypervisor, built for
  embedded IoT. As new platforms for embedded systems are rapidly introduced,
  support for one binary would require more and more complexity in the
  hypervisor, something we need to avoid.

* **Keep small footprint** Implementation of dynamic parsing would introduce
  hundreds or thousands of code. Avoiding dynamic parsing would help keep
  Lines of Code (LOC) of the hypervisor in a desirable range (around 30K).

* **Improve boot up time** Dynamic parsing at runtime would increase the boot
  up time. Using build-time configuration and not dynamic parsing would help
  improve boot up time of the hypervisor.


You can build the ACRN hypervisor, device model, and tools from
source, by following these steps.

.. _install-build-tools-dependencies:

Install build tools and dependencies
************************************

ACRN development is supported on Clear Linux distributions,

  .. note::
     ACRN uses ``menuconfig``, a python3 text-based user interface (TUI) for
     configuring hypervisor options and using python's ``kconfiglib`` library.

On the Clear Linux OS development system, install the necessary tools:

  .. code-block:: none

     $ sudo swupd bundle-add os-clr-on-clr os-core-dev python3-basic
     $ pip3 install --user kconfiglib


Get the ACRN hypervisor source code
***********************************

The `acrn-hypervisor <https://github.com/projectacrn/acrn-hypervisor/>`_
repository has four main components in it:

1. The ACRN hypervisor code located in the ``hypervisor`` directory
#. The EFI stub code located in the ``misc/efi-stub`` directory
#. The ACRN devicemodel code located in the ``devicemodel`` directory
#. The ACRN tools source code located in the ``misc/tools`` directory

Follow this step to get the acrn-hypervisor source code:

.. code-block:: none

   $ git clone https://github.com/projectacrn/acrn-hypervisor


Build with the ACRN scenario
****************************

Currently ACRN hypervisor defines these typical usage scenarios:

SDC:
   The SDC (Software Defined Cockpit) scenario defines a simple
   automotive use-case where there is one pre-launched Service VM and one
   post-launched User VM.

SDC2:
   SDC2 (Software Defined Cockpit 2) is an extended scenario for an
   automotive SDC system.  SDC2 defined one pre-launched Service VM and up
   to three post-launched VMs.

LOGICAL_PARTITION:
    This scenario defines two pre-launched VMs.

INDUSTRY:
   This is a typical scenario for industrial usage with up to four VMs:
   one pre-launched Service VM, one post-launched Standard VM for Human
   interaction (HMI), and one or two post-launched RT VMs for real-time
   control.

HYBRID:
   This scenario defines a hybrid use-case with three VMs: one
   pre-launched VM, one pre-launched Service VM, and one post-launched
   Standard VM.

Assuming that you are under the top-level directory of acrn-hypervisor:

* Build ``INDUSTRY`` scenario on ``nuc7i7dnb``:

  .. code-block:: none

     $ make all BOARD=nuc7i7dnb SCENARIO=industry

* Build ``SDC`` scenario on ``nuc6cayh``:

  .. code-block:: none

     $ make all BOARD=nuc6cayh SCENARIO=sdc


See the :ref:`hardware` document for information about the platform
needs for each scenario.

.. _getting-started-hypervisor-configuration:

Build with the hypervisor configuration
***************************************

Modify the hypervisor configuration
===================================

The ACRN hypervisor leverages Kconfig to manage configurations, powered by
Kconfiglib. A default configuration is generated based on the board you have
selected via the ``BOARD=`` command line parameter. You can make further
changes to that default configuration to adjust to your specific
requirements.

To generate hypervisor configurations, you need to build the hypervisor
individually. The following steps generate a default but complete configuration,
based on the platform selected, assuming that you are under the top-level
directory of acrn-hypervisor. The configuration file, named ``.config``, can be
found under the target folder of your build.

.. code-block:: none

   $ make defconfig BOARD=nuc6cayh

The BOARD specified is used to select a defconfig under
``arch/x86/configs/``. The other command-line based options (e.g. ``RELEASE``)
take no effects when generating a defconfig.

To modify the hypervisor configurations, you can either edit ``.config``
manually, or invoke a TUI-based menuconfig, powered by kconfiglib, by executing
``make menuconfig``. As an example, the following commands, assuming that you
are under the top-level directory of acrn-hypervisor, generate a default
configuration file for UEFI, allow you to modify some configurations and build
the hypervisor using the updated ``.config``.

.. code-block:: none

   # Modify the configurations per your needs
   $ cd ../         # Enter top-level folder of acrn-hypervisor source
   $ make menuconfig -C hypervisor BOARD=kbl-nuc-i7   <select industry scenario>

.. note::
   Menuconfig is python3 only.

Refer to the help on menuconfig for a detailed guide on the interface.

.. code-block:: none

   $ pydoc3 menuconfig

Build the hypervisor, device model and tools
============================================

Now you can build all these components in one go as follows:

.. code-block:: none

   $ make FIRMWARE=uefi          # Build the UEFI hypervisor with the new .config

The build results are found in the ``build`` directory.  You can specify
use a different Output folder by setting the ``O`` make parameter,
for example: ``make O=build-nuc BOARD=nuc6cayh``.

If you only need the hypervisor, then use this command:

.. code-block:: none

   $ make clean                       # Remove files previously built
   $ make -C hypervisor
   $ make -C misc/efi-stub HV_OBJDIR=$PWD/hypervisor/build EFI_OBJDIR=$PWD/hypervisor/build

The``acrn.efi`` will be generated in directory: ``./hypervisor/build/acrn.efi`` hypervisor.

As mentioned in :ref:`ACRN Configuration Tool <vm_config_workflow>`,
Board configuration and VM configuration could be imported from XML files.
If you want to build hypervisor with XML configuration files, please specify the
file location as follows:

.. code-block:: none

   $ BOARD_FILE=/home/acrn-hypervisor/misc/acrn-config/xmls/board-xmls/apl-up2.xml 
   SCENARIO_FILE=/home/acrn-hypervisor/misc/acrn-config/xmls/config-xmls/apl-up2/sdc.xml FIRMWARE=uefi

.. note:: The file path must be absolute path. Both of the ``BOARD`` and ``SCENARIO``
   parameters are not needed because the information could be got from XML.

Follow the same instructions to boot and test the images you created from your build.


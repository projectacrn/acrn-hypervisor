.. _hypervisor-make-options:

Hypervisor Makefile Options
###########################

The ACRN hypervisor source code provides a ``Makefile`` to build the ACRN
hypervisor binary and associated components.

Assuming that you are at the top level of the ``acrn-hypervisor`` directory,
you can run the ``make`` command to start the build. See
:ref:`acrn_configuration_tool` for information about required input files.

Build Options and Targets
**************************

The following table shows ACRN-specific command-line options:

.. list-table::
   :widths: 33 77
   :header-rows: 1

   * - Option
     - Description

   * - ``BOARD``
     - Required. Path to the board configuration file.

   * - ``SCENARIO``
     - Required. Path to the scenario configuration file.

   * - ``RELEASE``
     - Optional. Build a release version or a debug version. Valid values
       are ``y`` for release version or ``n`` for debug version. (Default
       is ``n``.)

   * - ``ASL_COMPILER``
     - Optional. Specify the path to the ``iasl`` compiler on the development machine.
       (If not provided, the default value is derived from ``which iasl``.)

   * - ``O``
     - Optional. Path to the directory where the built files will be stored.
       (Default is the ``build`` directory.)

The following table shows ACRN-specific targets. The default target (if no target is specified on the command-line) is to build the ``hypervisor``, ``devicemodel``, and ``tools``.

.. list-table::
   :widths: 33 77
   :header-rows: 1

   * - Makefile Target
     - Description

   * - ``hypervisor``
     - Optional. Build the hypervisor.

   * - ``devicemodel``
     - Optional. Build the Device Model. The ``tools`` will also be built as
       a dependency.

   * - ``tools``
     - Optional. Build the tools.

   * - ``doc``
     - Optional. Build the project's HTML documentation (using Sphinx), output
       to the ``build/doc`` folder.

   * - ``life_mngr``
     - Optional. Build the Lifecycle Manager daemon that runs in the User VM
       to manage power state transitions (S5).

   * - ``targz-pkg``
     - Optional. Create a compressed tarball (``acrn-$(FULL_VERSION).tar.gz``)
       in the build folder (default: ``build``) with all the build artifacts.

Example of a command to build the debug version:

.. code-block:: none

   make BOARD=~/acrn-work/my_board.xml SCENARIO=~/acrn-work/shared.xml

Example of a command to build the release version:

.. code-block:: none

   make BOARD=~/acrn-work/my_board.xml SCENARIO=~/acrn-work/shared.xml RELEASE=y

Example of a command to build the release version (hypervisor only):

.. code-block:: none

   make BOARD=~/acrn-work/my_board.xml SCENARIO=~/acrn-work/shared.xml RELEASE=y hypervisor

Example of a command to build the release version of the Device Model and tools:

.. code-block:: none

   make RELEASE=y devicemodel tools

Example of a command to put the built files in the specified directory
(``build-nuc``):

.. code-block:: none

   make O=build-nuc BOARD=~/acrn-work/my_board.xml SCENARIO=~/acrn-work/shared.xml

Example of a command that specifies ``iasl`` compiler:

.. code-block:: none

   make BOARD=~/acrn-work/my_board.xml SCENARIO=~/acrn-work/shared.xml ASL_COMPILER=/usr/local/bin/iasl

ACRN uses XML files to summarize board characteristics and scenario settings.
The ``BOARD`` and ``SCENARIO`` variables accept board/scenario names as well
as paths to XML files. When board/scenario names are given, the build system
searches for XML files with the same names under ``misc/config_tools/data/``.
When paths (absolute or relative) to the XML files are given, the build system
uses the files pointed at. If relative paths are used, they are considered
relative to the current working directory.

.. _acrn_makefile_targets:

Makefile Targets for Configuration
***********************************

ACRN source also includes the following makefile targets to aid customization.

.. list-table::
   :widths: 33 77
   :header-rows: 1

   * - Target
     - Description

   * - ``hvdefconfig``
     - Generate configuration files (a bunch of C source files) in the build
       directory without building the hypervisor. This target can be used when
       you want to customize the configurations based on a predefined scenario.

   * - ``hvshowconfig``
     - Print the target ``BOARD``, ``SCENARIO`` and build type (debug or
       release) of a build.

   * - ``hvdiffconfig``
     - After modifying the generated configuration files, you can use this
       target to generate a patch that shows the differences made.

   * - ``hvapplydiffconfig PATCH=/path/to/patch``
     - Register a patch to be applied on the generated configuration files
       every time they are regenerated. The ``PATCH`` variable specifies the
       path (absolute or relative to current working directory) of the patch.
       Multiple patches can be registered by invoking this target multiple
       times.

Example of ``hvshowconfig`` to query the board, scenario, and build
type of an existing build:

.. code-block:: none

   $ make BOARD=~/acrn-work/my_board.xml SCENARIO=~/acrn-work/shared.xml hypervisor
   ...
   $ make hvshowconfig
   Build directory: /path/to/acrn-hypervisor/build/hypervisor
   This build directory is configured with the settings below.
   - BOARD = my_board
   - SCENARIO = shared
   - RELEASE = n

Example of ``hvdefconfig`` to generate the configuration files in the
build directory, followed by an example of editing one of the configuration
files manually (``scenario.xml``) and then building the hypervisor:

.. code-block:: none

   make BOARD=nuc7i7dnb SCENARIO=shared hvdefconfig
   vim build/hypervisor/.scenario.xml
   #(Modify the XML file per your needs)
   make

A hypervisor build remembers the board and scenario previously configured.
Thus, there is no need to duplicate ``BOARD`` and ``SCENARIO`` in the second
``make`` above.

While the scenario configuration files can be changed manually, we recommend
you use the :ref:`ACRN Configurator tool <acrn_configurator_tool>`, which
provides valid options and descriptions of the configuration entries.

The targets ``hvdiffconfig`` and ``hvapplydiffconfig`` are provided for users
who already have offline patches to the generated configuration files. Prior to
v2.4, the generated configuration files are also in the repository. Some users
may already have chosen to modify these files directly to customize the
configurations.

.. note::

   We highly recommend new users save and maintain customized configurations in
   XML, not in patches to generated configuration files.

Example of how to use ``hvdiffconfig`` to generate a patch and save
it to ``config.patch``:

.. code-block:: console

   acrn-hypervisor$ make BOARD=ehl-crb-b SCENARIO=hybrid_rt hvdefconfig
   ...
   acrn-hypervisor$ vim build/hypervisor/configs/scenarios/hybrid_rt/pci_dev.c
   (edit the file manually)
   acrn-hypervisor$ make hvdiffconfig
   ...
   Diff on generated configuration files is available at /path/to/acrn-hypervisor/build/hypervisor/config.patch.
   To make a patch effective, use 'hvapplydiffconfig PATCH=/path/to/patch' to
   register it to a build.
   ...
   acrn-hypervisor$ cp build/hypervisor/config.patch config.patch

Example of how to use ``hvapplydiffconfig`` to apply
``config.patch`` to a new build:

.. code-block:: console

   acrn-hypervisor$ make clean
   acrn-hypervisor$ make BOARD=ehl-crb-b SCENARIO=hybrid_rt hvdefconfig
   ...
   acrn-hypervisor$ make hvapplydiffconfig PATCH=config.patch
   ...
   /path/to/acrn-hypervisor/config.patch is registered for build directory /path/to/acrn-hypervisor/build/hypervisor.
   Registered patches will be applied the next time 'make' is invoked.
   To unregister a patch, remove it from /path/to/acrn-hypervisor/build/hypervisor/configs/.diffconfig.
   ...
   acrn-hypervisor$ make hypervisor
   ...
   Applying patch /path/to/acrn-hypervisor/config.patch:
   patching file scenarios/hybrid_rt/pci_dev.c
   ...

Experimental (Multi-Arch Support)
*********************************

(WIP, Unstable)

ACRN also supports running on multiple architectures. To build hypervisor for
other architecture, add ``ARCH=<arch>``, and ``CROSS_COMPILE=<toolchainprefix>``.
For example:

.. code-block:: none

   $ make hypervisor BOARD=qemu SCENARIO=shared ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu-


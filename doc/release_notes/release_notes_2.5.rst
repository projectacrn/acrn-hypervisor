.. _release_notes_2.5:

ACRN v2.5 (Jun 2021) DRAFT
##########################

We are pleased to announce the release of the Project ACRN hypervisor
version 2.5.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline
embedded development through an open-source platform. See the
:ref:`introduction` introduction for more information.

All project ACRN source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, tools, and
documentation. You can either download this source code as a zip or
tar.gz file (see the `ACRN v2.5 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v2.5>`_) or
use Git ``clone`` and ``checkout`` commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v2.5

The project's online technical documentation is also tagged to
correspond with a specific release: generated v2.5 documents can be
found at https://projectacrn.github.io/2.5/.  Documentation for the
latest under-development branch is found at
https://projectacrn.github.io/latest/.

ACRN v2.5 requires Ubuntu 18.04.  Follow the instructions in the
:ref:`rt_industry_ubuntu_setup` to get started with ACRN.


What's New in v2.5
******************


ACRN Configuration and Build
============================

The following major changes on ACRN configuration and build process have been
integrated into v2.5:

- Change 1
- Change 2

For complete instructions to get started with the new build system, refer to
:ref:`getting-started-building`. For an introduction on the concepts and
workflow of the new configuration mechanism, refer to
:ref:`acrn_configuration_tool`.

Upgrading to v2.5 From Previous Releases
****************************************

We highly recommended that you follow the instructions below to
upgrade to v2.5 from previous ACRN releases.

Additional Dependencies
=======================

Python version 3.6 or higher is required to build ACRN v2.5. You can check the version of
Python you are using by:

.. code-block:: bash

   $ python3 --version
   Python 3.5.2

Only when the reported version is less than 3.6 (as is the case in the example above) do
you need an upgrade. The first (and preferred) choice is to install the latest
Python 3 from the official package repository:

.. code-block:: bash

   $ sudo apt install python3
   ...
   $ python --version
   Python 3.8.8

If this does not get you an appropriate version, you may use the deadsnakes PPA
(using the instructions below) or build from source yourself.

.. code-block:: bash

   $ sudo add-apt-repository ppa:deadsnakes/ppa
   $ sudo apt-get update
   $ sudo apt install python3.9
   $ python --version
   Python 3.9.2

In addition, the following new tools and packages are needed to build ACRN v2.5.
(If you're already using ACRN v2.4, you should already have these tools.)

.. code-block:: bash

   $ sudo apt install libxml2-utils xsltproc
   $ sudo pip3 install lxml xmlschema

.. note::
   This is not the complete list of tools required to build ACRN. Refer to
   :ref:`getting-started-building` for a complete guide to get started from
   scratch.

Configuration File Format
=========================

Starting with release v2.4, Kconfig is no longer used, and the contents of
scenario XML files have been simplified. If you're using v2.3 or earlier, you
need to upgrade your own Kconfig-format files or scenario XML files if you
maintain any.

See the instructions in the :ref:`release_notes_2.4` release notes for helpful
instructions to assist in this upgrade.


Build Commands
==============

We recommend you update the usage of variables ``BOARD_FILE`` and
``SCENARIO_FILE``, which are being deprecated,  and ``RELEASE``:

 - ``BOARD_FILE`` should be replaced with ``BOARD``. You should not specify
   ``BOARD`` and ``BOARD_FILE`` at the same time.
 - Similarly, ``SCENARIO_FILE`` should be replaced with ``SCENARIO``.
 - The value of ``RELEASE`` should be either ``y`` (previously was ``1``) or
   ``n`` (previously was ``0``).

``BOARD_FILE`` and ``SCENARIO_FILE`` can still be used but will take effect
only if ``BOARD`` and ``SCENARIO`` are not defined. They will be deprecated in
a future release.

Patches on Generated Sources
============================

The C files generated from board and scenario XML files were removed from the
repository in v2.4. Instead they will be generated in the build output when
building the hypervisor. See the instructions in the :ref:`release_notes_2.4`
release notes for more information.

Modifying generated files is not a recommended practice.
If you find a configuration that is not flexible enough to meet your
needs, please let us know by sending mail to `the acrn-dev mailing
list <https://lists.projectacrn.org/g/acrn-dev>`_ or submitting a
`GitHub issue <https://github.com/projectacrn/acrn-hypervisor/issues>`_.

Document Updates
****************

With the changes to ACRN configuration noted above, we made substantial updates
to the ACRN documentation around configuration and options, as listed here:

.. rst-class:: rst-columns2

* :ref:`contribute_guidelines`
* :ref:`doc_guidelines`
* :ref:`ahci-hld`
* :ref:`hv-device-passthrough`
* :ref:`hv-hypercall`
* :ref:`timer-hld`
* :ref:`l1tf`
* :ref:`modularity`
* :ref:`sw_design_guidelines`
* :ref:`trusty_tee`
* :ref:`getting-started-building`
* :ref:`gsg`
* :ref:`hardware`
* :ref:`acrn_configuration_tool`
* :ref:`acrn_on_qemu`
* :ref:`acrn_doc`
* :ref:`enable_ivshmem`
* :ref:`enable-ptm`
* :ref:`nested_virt`
* :ref:`running_deb_as_serv_vm`
* :ref:`trusty-security-services`
* :ref:`using_hybrid_mode_on_nuc`
* :ref:`connect_serial_port`
* :ref:`acrn-dm_parameters`
* :ref:`kernel-parameters`

We've also made edits throughout the documentation to improve clarity,
formatting, and presentation throughout the ACRN documentation.

Fixed Issues Details
********************

.. comment example item
   - :acrn-issue:`5626` - [CFL][industry] Host Call Trace once detected

Known Issues
************


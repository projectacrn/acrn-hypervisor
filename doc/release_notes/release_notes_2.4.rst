.. _release_notes_2.4:

ACRN v2.4 (Mar 2021)
####################

We are pleased to announce the release of the Project ACRN hypervisor
version 2.4.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline
embedded development through an open-source platform. See the
:ref:`introduction` introduction for more information.  All project ACRN
source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, tools, and
documentation. You can either download this source code as a zip or
tar.gz file (see the `ACRN v2.4 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v2.4>`_) or
use the Git ``clone`` and ``checkout`` commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v2.4

The project's online technical documentation is also tagged to
correspond with a specific release: generated v2.4 documents can be
found at https://projectacrn.github.io/2.4/.  Documentation for the
latest under-development branch is found at
https://projectacrn.github.io/latest/.

ACRN v2.4 requires Ubuntu 18.04.  Follow the instructions in the
:ref:`rt_industry_ubuntu_setup` to get started with ACRN.


What's New in v2.4
******************

ACRN Configuration and Build
============================

The following major changes on ACRN configuration and build process have been
integrated into v2.4:

 - Metadata of configuration entries, including documentation and attributes,
   has been removed from ``scenario`` XMLs.
 - The C sources generated from ``board`` and ``scenario`` XMLs are no longer
   maintained in the repository. Instead they'll be generated as part of the
   hypervisor build. Users can now find them under ``configs/`` of the build
   directory.
 - Kconfig is no longer used for configuration. Related build targets, such as
   ``defconfig``, now apply to the configuration files in XML.
 - The ``make`` command-line variables ``BOARD`` and ``BOARD_FILE`` have been
   unified. Users can now specify ``BOARD=xxx`` when invoking ``make`` with
   ``xxx`` being either a board name or a (relative or absolute) path to a
   board XML file. ``SCENARIO`` and ``SCENARIO_FILE`` have been unified in the same
   way.

For complete instructions to get started with the new build system, refer to
:ref:`getting-started-building`. For an introduction on the concepts and
workflow of the new configuration mechanism, refer to
:ref:`acrn_configuration_tool`.

Upgrading to v2.4 From Previous Releases
****************************************

We highly recommended that you follow the instructions below to
upgrade to v2.4 from previous ACRN releases.

Additional Dependencies
=======================

Python version 3.6 or higher is required to build ACRN v2.4. You can check the version of
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

In addition, the following new tools and packages are needed to build ACRN v2.4:

.. code-block:: bash

   $ sudo apt install libxml2-utils xsltproc
   $ sudo pip3 install lxml xmlschema

.. note::
   This is not the complete list of tools required to build ACRN. Refer to
   :ref:`getting-started-building` for a complete guide to get started from
   scratch.

Configuration File Format
=========================

Starting with release v2.4, Kconfig is no longer used, and the contents of scenario
XML files have been simplified. You need to upgrade your own Kconfig-format files
or scenario XML files if you maintain any.

For Kconfig-format file, you must translate your configuration to a scenario
XML file where all previous Kconfig configuration entries are also available. Refer
to :ref:`scenario-config-options` for the full list of settings available in
scenario XML files.

For scenario XML files, you need to remove the obsolete metadata in those files. You can use
the following XML transformation (in XSLT) for this purpose:

.. code-block:: xml

   <?xml version="1.0" encoding="utf-8"?>
   <xsl:stylesheet
       version="1.0"
       xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

     <xsl:template match="@desc" />
     <xsl:template match="@configurable | @multiselect | @readonly" />

     <!-- The identity template -->
     <xsl:template match="@*|node()">
       <xsl:copy>
         <xsl:apply-templates select="@*|node()"/>
       </xsl:copy>
     </xsl:template>
   </xsl:stylesheet>

After saving the snippet above to a file (e.g., ``remove_metadata.xsl``), you
can use ``xsltproc`` to clean and transform your own scenario XML file:

.. code-block:: bash

   $ xsltproc -o <path/to/output> remove_metadata.xsl <path/to/your/XML>

New Configuration Options
=========================

The following element is added to scenario XML files in v2.4:

 - :option:`hv.FEATURES.ENFORCE_TURNOFF_AC`

To upgrade a v2.3-compliant scenario XML file, you can use the following XML
transformation. The indentation in this transformation are carefully tweaked for
the best indentation in converted XML files.

.. code-block:: xml

   <?xml version="1.0" encoding="utf-8"?>
   <xsl:stylesheet
       version="1.0"
       xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
     <xsl:template match="hv/FEATURES/MULTIBOOT2">
       <xsl:copy>
         <xsl:apply-templates select="@*|node()"/>
       </xsl:copy>
       <xsl:if test="not(../ENFORCE_TURNOFF_AC)">
         <xsl:text>
               </xsl:text>
         <ENFORCE_TURNOFF_AC>y</ENFORCE_TURNOFF_AC>
       </xsl:if>
     </xsl:template>

     <!-- The identity template -->
     <xsl:template match="@*|node()">
       <xsl:copy>
         <xsl:apply-templates select="@*|node()"/>
       </xsl:copy>
     </xsl:template>
   </xsl:stylesheet>

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

The C files generated from board and scenario XML files have been removed from the
repository in v2.4. Instead they will be generated in the build output when building the
hypervisor.

Typically you should be able to customize your scenario by modifying the
scenario XML file rather than the generated files directly. But if that is not
possible, you can still register one or more patches that will be applied to
the generated files by following the instructions in
:ref:`acrn_makefile_targets`.

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

* :ref:`hv-config`
* :ref:`scenario-config-options`
* :ref:`acrn_configuration_tool`
* :ref:`vuart_config`
* :ref:`getting-started-building`
* :ref:`acrn-dm_parameters`
* :ref:`kernel-parameters`

Additional new or updated reference documents are also available, including:

.. rst-class:: rst-columns2

* :ref:`rt_industry_ubuntu_setup`
* :ref:`setup_openstack_libvirt`
* :ref:`using_windows_as_uos`

We've also made edits throughout the documentation to improve clarity,
formatting, and presentation throughout the ACRN documentation.

Deprivileged Boot Mode Support
==============================

Because we dropped deprivileged boot mode support (in v2.3), we also
switched our Service VM of choice away from Clear Linux and have
removed Clear Linux-specific tutorials.  Deleted documents are still
available in the `version-specific v2.1 documentation
<https://projectacrn.github.io/v2.1/>`_.


Fixed Issues Details
********************

.. example - :acrn-issue:`4958` - clean up spin lock for hypervisor

Known Issues
************

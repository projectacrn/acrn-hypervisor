.. _scenario-config-options:

Scenario Configuration Options
##############################

As explained in :ref:`acrn_configuration_tool`, ACRN scenarios define
the hypervisor (hv) and VM settings for the execution environment of an
ACRN-based application.  This document describes these option settings.

.. rst-class:: rst-columns3

.. contents::
   :local:
   :depth: 2

Common Option Value Types
*************************

Within this option documentation, we refer to some common type
definitions:

Boolean
  A true or false value displayed as a check box, checked indicating true.

Hexadecimal
  A base-16 (integer) value represented by a leading ``0x`` or ``0X`` followed by
  one or more characters ``0`` to ``9``, ``a`` to ``f``, or ``A`` to ``F``.

Integer
  A base-10 value represented by the characters ``0`` to ``9``.  The
  first character must not be a ``0``. Only positive values are
  expected.

String
  A sequence of UTF-8 characters.  String-length limits or specific
  string value restrictions are defined in the option description.

.. comment These images are used in generated option documentation

.. |icon-advanced| image:: images/Advanced.png
   :alt: Find this option on the Configurator's Advanced Parameters tab
.. |icon-basic| image:: images/Basic.png
   :alt: Find this option on the Configurator's Basic Parameters tab
.. |icon-not-available| image:: images/Not-available.png
   :alt: This is a hidden option and not user-editable using the Configurator
.. |icon-post-launched-vm| image:: images/Post-launched-VM.png
   :alt: Find this option on a Configurator Post-launched VM tab
.. |icon-pre-launched-vm| image:: images/Pre-launched-VM.png
   :alt: Find this option on a Configurator Pre-launched VM tab
.. |icon-service-vm| image:: images/Service-VM.png
   :alt: Find this option on the Configurator Service VM tab
.. |icon-hypervisor| image:: images/Hypervisor.png
   :alt: Find this option on the Configurator's Hypervisor Global Settings tab

We use icons within an option description to indicate where the option can be
found within the Configurator UI:

.. list-table::
   :header-rows: 1

   * - **Hypervisor/VM Tab**
     - **Basic/Advanced Tab**

   * - |icon-hypervisor|
         Find this option on the Configurator's **Hypervisor Global Settings** tab
     - |icon-basic|
           Find this option on the Hypervisor's or VM's **Basic Parameters** tab

   * - |icon-pre-launched-vm|
          Find this option on a Configurator **Pre-Launched VM** tab
     - |icon-advanced|
           Find this option on the Hypervisor's or VM's **Advanced Parameters** tab

   * - |icon-post-launched-vm|
           Find this option on a Configurator **Post-Launched VM** tab
     -

   * - |icon-service-vm|
           Find this option on the Configurator **Service VM** tab
     -

-----

.. comment This configdoc.txt is generated during the doc build process
   from the acrn config schema files found in misc/config_tools/schema

.. include:: configdoc.txt

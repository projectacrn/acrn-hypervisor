.. _scenario-config-options:

Scenario Configuration Options
##############################

As explained in :ref:`acrn_configuration_tool`, ACRN scenarios define
the hypervisor (hv) and VM settings for the execution environment of an
ACRN-based application.  This document describes these option settings.

.. contents::
   :local:
   :depth: 2

Common option value types
*************************

Within this option documentation, we refer to some common type
definitions:

Boolean
  A true or false value specified as either ``y`` or ``n``. Other
  values such as ``t`` or ``f`` are not supported.

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


.. comment This configdoc.txt is generated during the doc build process
   from the acrn config schema files found in misc/config_tools/schema

.. include:: configdoc.txt

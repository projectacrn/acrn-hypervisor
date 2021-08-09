.. _release_notes_2.5.1:

ACRN v2.5.1 (Aug 2021)
######################

We are pleased to announce an update release of the Project ACRN hypervisor
version 2.5.1. This is a maintenance release based off the v2.5 branch that
provides an update with a few bug fixes and support for Trusted Platform Module
(TPM) 2.0.

There were no documentation changes in this update, so you can still refer to
the v2.5-specific documentation found at https://projectacrn.github.io/2.5/.

See the :ref:`release_notes_2.5` release notes for information about the v2.5
release.


What's New in v2.5.1
********************

Trusted Platform Module (TPM) 2.0 support
  ACRN hypervisor now supports pre-launched VM remote attestation use cases by
  passing the TPM 2.0 ACPI table provided by the BIOS to the pre-launched VM.

Fixed Issues Details
********************

.. comment example item
   - :acrn-issue:`5626` - [CFL][industry] Host Call Trace once detected

- :acrn-issue:`6305` - Pre-launched Zephyr VM coud not get ACPI table
- :acrn-issue:`6334` - Hypervisor console does not support 64-bit bar PCI UART with 32-bit bar space

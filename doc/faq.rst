.. _faq:

FAQ
###

Here are some frequently asked questions about the ACRN project.

.. contents::
   :local:
   :backlinks: entry

------

What hardware does ACRN support?
********************************

ACRN runs on Intel Apollo Lake and Kaby Lake boards, as documented in
our :ref:`hardware` documentation.

Clear Linux* fails to boot on my NUC
************************************

If you're following the :ref:`getting_started` documentation and the NUC
fails to boot, here are some options to try:

* Upgrade your platform to the latest BIOS
* Verify Secure Boot is disabled in the BIOS settings:

  - Depending on your platform, press for example, :kbd:`F2` while
    booting to enter the BIOS options menu, and verify "Secure Boot" is
    not checked in the "Boot Options"
* Make sure you are using EFI (and not legacy BIOS)

How do I configure ACRN's memory use?
*************************************

It's important that the ACRN Kconfig settings are aligned with the physical memory
on your platform. Check the documentation for these option settings for
details:

* :option:`CONFIG_PLATFORM_RAM_SIZE`
* :option:`CONFIG_SOS_RAM_SIZE`
* :option:`CONFIG_UOS_RAM_SIZE`
* :option:`CONFIG_HV_RAM_SIZE`

For example, if memory is 32G, setup ``PLATFORM_RAM_SIZE`` = 32G

::

  config PLATFORM_RAM_SIZE
        hex "Size of the physical platform RAM"
        default 0x200000000 if PLATFORM_SBL
        default 0x800000000 if PLATFORM_UEFI

Setup ``SOS_RAM_SIZE`` = 32G too (The SOS will have the whole resource)

::

  config SOS_RAM_SIZE
        hex "Size of the vm0 (SOS) RAM"
        default 0x200000000 if PLATFORM_SBL
        default 0x800000000 if PLATFORM_UEFI

Setup ``UOS_RAM_SIZE`` to what you need, for example,  16G

::

  config UOS_RAM_SIZE
        hex "Size of the User OS (UOS) RAM"
        default 0x100000000 if PLATFORM_SBL
        default 0x400000000 if PLATFORM_UEFI

Setup ``HV_RAM_SIZE`` (we will reserve memory for guest EPT paging
table), if you setup 32G (default 16G), you must enlarge it with
(32G-16G)/2M pages (where pages are 4K). The example below is after
HV_RAM_SIZE is changed to 240M

::

  config HV_RAM_SIZE
    hex "Size of the RAM region used by the hypervisor"
    default 0x07800000 if PLATFORM_SBL
    default 0x0f000000 if PLATFORM_UEFI

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

Clear Linux* OS fails to boot on my NUC
***************************************

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
        hex "Size of the Service OS (SOS) RAM"
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

How to modify the default display output for a UOS?
***************************************************

Apollo Lake HW has three pipes and each pipe can have three or four planes which
help to display the overlay video. The hardware can support up to 3 monitors
simultaneously. Some parameters are available to control how display monitors
are assigned between the SOS and UOS(s), simplifying the assignment policy and
providing configuration flexibility for the pipes and planes for various IoT
scenarios. This is known as the **plane restriction** feature.

* ``i915.avail_planes_per_pipe``: for controlling how planes are assigned to the
  pipes
* ``i915.domain_plane_owners``: for controlling which domain (VM) will have
  access to which plane

Refer to :ref:`GVT-g-kernel-options` for detailed parameter descriptions.

In the default configuration, pipe A is assigned to the SOS and pipes B and C
are assigned to the UOS, as described by these parameters:

* SOS::

    i915.avail_planes_per_pipe=0x01010F
    i915.domain_plane_owners=0x011111110000

* UOS::

    i915.avail_planes_per_pipe=0x0070F00

To assign pipes A and B to the UOS, while pipe C is assigned to the SOS, use
these parameters:

* SOS::

    i915.avail_planes_per_pipe=0x070101
    i915.domain_plane_owners=0x000011111111

* UOS::

    i915.avail_planes_per_pipe=0x000F0F

.. note::

   The careful reader may have noticed that in all examples given above, the SOS
   always has at least one plane per pipe. This is intentional, and the driver
   will enforce this if the parameters do not do this.

Why does ACRN need to know how much RAM the system has?
*******************************************************

Configuring ACRN at compile time with the system RAM size is a tradeoff between
flexibility and functional safety certification. For server virtualization, one
binary is typically used for all platforms with flexible configuration options
given at run time. But, for IoT applications, the image is typically configured
and built for a particular product platform and optimized for that product.

Important features for ACRN include Functional Safety (FuSa) and real-time
behavior. FuSa requires a static allocation policy to avoid the potential of
dynamic allocation failures. Real-time applications similarly benefit from
static memory allocation. This is why ACRN removed all ``malloc()``-type code,
and why it needs to pre-identify the size of all buffers and structures used in
the Virtual Memory Manager. For this reason, knowing the available RAM size at
compile time is necessary to statically allocate memory usage.

.. _faq:

Frequently Asked Questions
##########################

Here are some frequently asked questions about the ACRN project.

.. contents::
   :local:
   :backlinks: entry

------

What hardware does ACRN support?
********************************

ACRN runs on Intel boards, as documented in
our :ref:`hardware` documentation.

.. _config_32GB_memory:

How do I configure ACRN's memory size?
**************************************

It's important that the ACRN configuration settings are aligned with the
physical memory on your platform. Check the documentation for these
option settings for details:

* :option:`hv.MEMORY.PLATFORM_RAM_SIZE`
* :option:`hv.MEMORY.SOS_RAM_SIZE`
* :option:`hv.MEMORY.UOS_RAM_SIZE`
* :option:`hv.MEMORY.HV_RAM_SIZE`

For example, if the Intel NUC's physical memory size is 32G, you may follow these steps
to make the new UEFI ACRN hypervisor, and then deploy it onto the Intel NUC to boot
the ACRN Service VM with the 32G memory size.

#. Use ``make menuconfig`` to change the ``RAM_SIZE``::

   $ cd acrn-hypervisor
   $ make menuconfig -C hypervisor BOARD=nuc7i7dnb

#. Navigate to these items and then change the value as given below::

   (0x0f000000) Size of the RAM region used by the hypervisor
   (0x800000000) Size of the physical platform RAM
   (0x800000000) Size of the Service OS (SOS) RAM

#. Press :kbd:`S` and then :kbd:`Enter` to save the ``.config`` to the default directory:
   ``acrn-hypervisor/hypervisor/build/.config``

#. Press :kbd:`ESC` to leave the menu.

#. Then continue building the ACRN Service VM as usual.

How to modify the default display output for a User VM?
*******************************************************

Apollo Lake HW has three pipes and each pipe can have three or four planes which
help to display the overlay video. The hardware can support up to 3 monitors
simultaneously. Some parameters are available to control how display monitors
are assigned between the Service VM and User VM(s), simplifying the assignment policy and
providing configuration flexibility for the pipes and planes for various IoT
scenarios. This is known as the **plane restriction** feature.

* ``i915.avail_planes_per_pipe``: for controlling how planes are assigned to the
  pipes
* ``i915.domain_plane_owners``: for controlling which domain (VM) will have
  access to which plane

Refer to :ref:`GVT-g-kernel-options` for detailed parameter descriptions.

In the default configuration, pipe A is assigned to the Service VM and pipes B and C
are assigned to the User VM, as described by these parameters:

* Service VM::

    i915.avail_planes_per_pipe=0x01010F
    i915.domain_plane_owners=0x011111110000

* User VM::

    i915.avail_planes_per_pipe=0x0070F00

To assign pipes A and B to the User VM, while pipe C is assigned to the Service VM, use
these parameters:

* Service VM::

    i915.avail_planes_per_pipe=0x070101
    i915.domain_plane_owners=0x000011111111

* User VM::

    i915.avail_planes_per_pipe=0x000F0F

.. note:: The Service VM always has at least one plane per pipe. This is
   intentional, and the driver will enforce this if the parameters do not
   do this.

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

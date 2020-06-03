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

.. _config_32GB_memory:

How do I configure ACRN's memory size?
**************************************

It's important that the ACRN Kconfig settings are aligned with the physical memory
on your platform. Check the documentation for these option settings for
details:

* :option:`CONFIG_PLATFORM_RAM_SIZE`
* :option:`CONFIG_SOS_RAM_SIZE`
* :option:`CONFIG_UOS_RAM_SIZE`
* :option:`CONFIG_HV_RAM_SIZE`

For example, if the NUC's physical memory size is 32G, you may follow these steps
to make the new uefi ACRN hypervisor, and then deploy it onto the NUC board to boot
ACRN Service VM with the 32G memory size.

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

#. Use these command lines to build the new efi image for KBL NUC::

   $ make -C hypervisor
   $ make -C misc/efi-stub HV_OBJDIR=$PWD/hypervisor/build EFI_OBJDIR=$PWD/hypervisor/build

#. Log in to your KBL NUC (assumes all the ACRN configurations are set up), then copy
   the new efi image into the EFI partition::

   # mount /dev/sda1 /mnt
   # scp -r <user name>@<host address>:<your workspace>/acrn-hypervisor/hypervisor/build/acrn.efi /mnt/EFI/acrn/
   # sync && umount /mnt

#. Reboot KBL NUC to enjoy the ACRN with 32G memory.

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

.. note::

   The careful reader may have noticed that in all examples given above, the Service VM
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


How to build ACRN on Fedora 29?
*******************************

There is a known issue when attempting to build ACRN on Fedora 29
because of how ``gnu-efi`` is packaged in this Fedora release.
(See the `ACRN GitHub issue
<https://github.com/projectacrn/acrn-hypervisor/issues/2457>`_
for more information.)  The following patch to ``/efi-stub/Makefile``
fixes the problem on Fedora 29 development systems (but should
not be used on other Linux distros)::

   diff --git a/efi-stub/Makefile b/efi-stub/Makefile
   index 5b87d49b..dfc64843 100644
   --- a/efi-stub/Makefile
   +++ b/efi-stub/Makefile
   @@ -52,14 +52,14 @@ endif
    # its tools and libraries in different folders. The next couple of
    # variables will determine and set the right path for both the
    # tools $(GNUEFI_DIR) and libraries $(LIBDIR)
   -GNUEFI_DIR := $(shell find $(SYSROOT)/usr/lib* -name elf_$(ARCH)_efi.lds -type f | xargs dirname)
   +GNUEFI_DIR := $(shell find $(SYSROOT)/usr/lib* -name elf_x64_efi.lds -type f | xargs dirname)
   LIBDIR := $(subst gnuefi,,$(GNUEFI_DIR))
   -CRT0 := $(GNUEFI_DIR)/crt0-efi-$(ARCH).o
   -LDSCRIPT := $(GNUEFI_DIR)/elf_$(ARCH)_efi.lds
   +CRT0 := $(GNUEFI_DIR)/crt0-efi-x64.o
   +LDSCRIPT := $(GNUEFI_DIR)/elf_x64_efi.lds

    INCDIR := $(SYSROOT)/usr/include

   -CFLAGS=-I. -I.. -I../hypervisor/include/arch/x86/guest -I$(INCDIR)/efi -I$(INCDIR)/efi/$(ARCH) \
   +CFLAGS=-I. -I.. -I../hypervisor/include/arch/x86/guest -I$(INCDIR)/efi -I$(INCDIR)/efi/x64 \
                    -I../hypervisor/include/public -I../hypervisor/include/lib -I../hypervisor/bsp/include/uefi \
                    -DEFI_FUNCTION_WRAPPER -fPIC -fshort-wchar -ffreestanding \
                    -Wall -I../fs/ -D$(ARCH) -O2 \

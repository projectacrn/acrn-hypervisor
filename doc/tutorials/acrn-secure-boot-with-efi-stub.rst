.. _how-to-enable-acrn-secure-boot-with-efi-stub:

Enable ACRN Secure Boot With EFI-Stub
#####################################

Introduction
************

``ACRN EFI-Stub`` is an EFI application to support booting ACRN Hypervisor on
UEFI systems with Secure Boot. ACRN has supported
:ref:`how-to-enable-acrn-secure-boot-with-grub`.
It relies on the GRUB multiboot2 module by default. However, on certain platform
the GRUB multiboot2 is intentionally disabled when Secure Boot is enabled due
to the `CVE-2015-5281 <https://www.cvedetails.com/cve/CVE-2015-5281/>`_.

As an alternative booting method, ``ACRN EFI-Stub`` supports to boot ACRN HV on
UEFI systems without using GRUB. Although it is based on the legacy EFI-Stub
which was obsoleted in ACRN v2.3, the new EFI-Stub can boot ACRN HV in the direct
mode rather than the former deprivileged mode.

In order to boot ACRN HV with the new EFI-Stub, you need to create a container blob
which contains HV image and Service VM kernel image (and optionally pre-launched
VM kernel image and ACPI table). That blob file is stitched to the
EFI-Stub to form a single EFI application (``acrn.efi``). The overall boot flow is as below.

.. graphviz::

   digraph G {
      rankdir=LR;
      bgcolor="transparent";
      UEFI -> "acrn.efi" ->
      "ACRN\nHypervisor" -> "pre-launched RTVM\nKernel";
      "ACRN\nHypervisor" -> "Service VM\nKernel";
   }

- UEFI firmware verifies ``acrn.efi``
- ``acrn.efi`` unpacks ACRN Hypervisor image and VM Kernels from a stitched container blob
- ``acrn.efi`` loads ACRN Hypervisor to memory
- ``acrn.efi`` prepares MBI to store Service VM & pre-launched RTVM Kernel info
- ``acrn.efi`` hands over control to ACRN Hypervisor with MBI
- ACRN Hypervisor boots Service VM and pre-launched RTVM in parallel

As the container blob format, ``ACRN EFI-Stub`` uses the `Slim Bootloader Container
Boot Image <https://slimbootloader.github.io/how-tos/create-container-boot-image.html>`_.

Verified Configurations
***********************
- ACRN Hypervisor Release Version 2.5
- hybrid_rt scenario
- TGL platform
- CONFIG_MULTIBOOT2=y (as default)
- CONFIG_RELOC=y (as default)

Building
********

Build Dependencies
==================

- Build Tools and Dependencies described in the :ref:`gsg` guide
- ``gnu-efi`` package
- Service VM Kernel ``bzImage``
- pre-launched RTVM Kernel ``bzImage``
- `Slim Bootloader Container Tool <https://slimbootloader.github.io/how-tos/create-container-boot-image.html>`_

The Slim Bootloader Tools can be downloaded from its `GitHub project <https://github.com/slimbootloader/slimbootloader>`_.
The verified version is the commit `9f146af <https://github.com/slimbootloader/slimbootloader/tree/9f146af>`_.
You may use the `meta-acrn Yocto Project integration layer
<https://github.com/intel/meta-acrn>`_ to build Service VM Kernel and
pre-launched VM.

Build EFI-Stub for TGL hybrid_rt
======================================

.. code-block:: none

   $ TOPDIR=`pwd`
   $ cd acrn-hypervisor
   $ make BOARD=tgl-rvp SCENARIO=hybrid_rt hypervisor
   $ make BOARD=tgl-rvp SCENARIO=hybrid_rt -C misc/efi-stub/ \
     HV_OBJDIR=`pwd`/build/hypervisor/ \
     EFI_OBJDIR=`pwd`/build/hypervisor/misc/efi-stub `pwd`/build/hypervisor/misc/efi-stub/boot.efi

Create Container
================

.. code-block:: none

   $ mkdir -p $TOPDIR/acrn-efi; cd $TOPDIR/acrn-efi
   $ echo > hv_cmdline.txt
   $ echo RT_bzImage    > vm0_tag.txt
   $ echo Linux_bzImage > vm1_tag.txt
   $ echo ACPI_VM0      > acpi_vm0.txt

   $ python3 GenContainer.py create -cl \
     CMDL:./hv_cmdline.txt \
     ACRN:$TOPDIR/acrn-hypervisor/build/hypervisor/acrn.32.out \
     MOD0:./vm0_tag.txt  \
     MOD1:./vm0_kernel   \
     MOD2:./vm1_tag.txt  \
     MOD3:./vm1_kernel   \
     MOD4:./acpi_vm0.txt \
     MOD5:$TOPDIR/acrn-hypervisor/build/hypervisor/acpi/ACPI_VM0.bin \
     -o sbl_os    \
     -t MULTIBOOT \
     -a NONE

You may optionally put HV boot options in the ``hv_cmdline.txt`` file. This file
must contain at least one character even if you don't need additional boot options.

.. code-block:: none

   # Acceptable Examples
   $ echo     > hv_cmdline.txt    # end-of-line
   $ echo " " > hv_cmdline.txt    # space + end-of-line

   # Not Acceptable Example
   $ touch hv_cmdline.txt         # empty file

The ``vm0_kernel`` is the Kernel ``bzImage`` of the pre-launched RTVM, and the
``vm1_kernel`` is the image of the Service VM in the above case.

Stitch Container to EFI-Stub
============================

.. code-block:: none

   $ objcopy --add-section .hv=sbl_os --change-section-vma .hv=0x6e000 \
     --set-section-flags .hv=alloc,data,contents,load \
     --section-alignment 0x1000 $TOPDIR/acrn-hypervisor/build/hypervisor/misc/efi-stub/boot.efi acrn.efi

Installing (without SB for testing)
***********************************
For example:

.. code-block:: none

   $ sudo mkdir -p /boot/EFI/BOOT/
   $ sudo cp acrn.efi /boot/EFI/BOOT/
   $ sudo efibootmgr -c -l "\EFI\BOOT\acrn.efi" -d /dev/nvme0n1 -p 1 -L "ACRN Hypervisor"
   $ sudo reboot

Signing
*******
See :ref:`how-to-enable-acrn-secure-boot-with-grub` for how to sign your ``acrn.efi`` file.


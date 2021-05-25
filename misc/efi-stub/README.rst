ACRN v2.5 EFI-Stub
##################

Introduction
************

``ACRN v2.5 EFI-Stub`` is an EFI application to support booting ACRN Hypervisor on
UEFI systems with Secure Boot. ACRN has supported `Secure Boot With GRUB
<https://projectacrn.github.io/latest/tutorials/acrn-secure-boot-with-grub.html>`_.
It relies on the GRUB multiboot2 module by default. However, on certain platform
the GRUB multiboot2 is intentionally disabled when Secure Boot is enabled due
to the `CVE-2015-5281 <https://www.cvedetails.com/cve/CVE-2015-5281/>`_.

As an alternative booting method, ``ACRN v2.5 EFI-Stub`` supports to boot ACRN HV
on UEFI systems without using GRUB. Although it is based on the legacy EFI-Stub
which was obsoleted in ACRN v2.3, the new EFI-Stub can boot ACRN HV in the direct
mode rather than the former de-privilege mode.

In order to boot ACRN HV with the new EFI-Stub, you need to create a container blob
which contains HV image and Service VM kernel image (and optionally pre-launched
VM kernel image and ACPI table). That blob file is supposed to be stitched to the
EFI-Stub to form single EFI application (acrn.efi). The overall boot flow is as below.

UEFI BIOS/shim -> acrn.efi -> ACRN HV -> Service VM (pre-launched VM in parallel)

1. UEFI BIOS/shim verifies & loads a signed acrn.efi
2. acrn.efi unpacks ACRN HV image and VM kernels from a stitched container blob
3. acrn.efi loads ACRN HV to memory
4. acrn.efi prepares MBI to stores Service VM & pre-launched VM kernel info
5. acrn.efi hands over control to ACRN HV with MBI
6. ACRN HV boots Service VM (and pre-launched VM in parallel)

As the container blob format, ``ACRN v2.5 EFI-Stub`` uses the `Slim Bootloader
Container Boot Image <https://slimbootloader.github.io/how-tos/create-container-boot-image.html>`_.

Verified Configurations
***********************
- ACRN Hypervisor Release Version 2.5
- hybrid_rt scenario
- TGL-U RVP board
- CONFIG_MULTIBOOT2=y (as default)
- CONFIG_RELOC=y (as default)


Building
********

Build Dependencies
==================

- Build Tools and Dependencies described in the `Getting Started Guide <https://projectacrn.github.io/latest/getting-started/building-from-source.html>`_
- gnu-efi package
- Service VM Kernel bzImage
- pre-launched RTVM Kernel bzImage
- `Slim Bootloader Container Tool <https://slimbootloader.github.io/how-tos/create-container-boot-image.html>`_

The Slim Bootloader Tools can be downloaded from its github `project <https://github.com/slimbootloader/slimbootloader>`_.
The verified version is the commit `9f146afd47 <https://github.com/slimbootloader/slimbootloader/tree/9f146afd47e0ca204521826a583d55388850b216>`_.
You may use the `meta-acrn <https://github.com/intel/meta-acrn>`_ to build Service VM Kernel and pre-launched one.

Build EFI-Stub for TGL-U RVP hybrid_rt
======================================

.. code-block:: none

   $ cd acrn-hypervisor
   $ make BOARD=tgl-rvp SCENARIO=hybrid_rt hypervisor
   $ make BOARD=tgl-rvp SCENARIO=hybrid_rt -C misc/efi-stub/ \
     HV_OBJDIR=`pwd`/build/hypervisor/ EFI_OBJDIR=`pwd`/build/hypervisor/misc/efi-stub `pwd`/build/hypervisor/misc/efi-stub/boot.efi

Create Container
================

.. code-block:: none

   $ touch hv_cmdline.txt
   $ echo RT_bzImage    > vm0_tag.txt
   $ echo Linux_bzImage > vm1_tag.txt
   $ echo ACPI_VM0      > acpi_vm0.txt

   $ python3 GenContainer.py create -cl \
     CMDL:./hv_cmdline.txt \
     ACRN:build/hypervisor/acrn.32.out \
     MOD0:./vm0_tag.txt \
     MOD1:./vm0_kernel \
     MOD2:./vm1_tag.txt \
     MOD3:./vm1_kernel \
     MOD4:./acpi_vm0.txt \
     MOD5:build/hypervisor/acpi/ACPI_VM0.bin \
     -o sbl_os \
     -t MULTIBOOT \
     -a NONE

You may optionally put HV boot options to the hv_cmdline.txt. The vm0_kernel is the Kernel bzImage of the pre-launched RTVM,
and the vm1_kernel is the one of the Service VM in the above case.

Stitch Container to EFI-Stub
============================

.. code-block:: none

   $ objcopy --add-section .hv=sbl_os --change-section-vma .hv=0x6e000 --set-section-flags .hv=alloc,data,contents,load \
     --section-alignment 0x1000 ./build/hypervisor/misc/efi-stub/boot.efi acrn.efi

Installing (w/o SB for testing)
*******************************
For example:

.. code-block:: none

   $ sudo mkdir -p /boot/EFI/acrn
   $ sudo cp acrn.efi /boot/EFI/
   $ sudo efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L "ACRN Hypervisor"

Signing
*******
See the page `Enable ACRN Secure Boot With GRUB <https://projectacrn.github.io/latest/tutorials/acrn-secure-boot-with-grub.html>`_
for how to sign your acrn.efi file.

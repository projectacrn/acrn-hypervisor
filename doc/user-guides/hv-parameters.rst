.. _hv-parameters:

ACRN Hypervisor Parameters
##########################

Generic Hypervisor Parameters
*****************************

The ACRN hypervisor supports the following parameter:

+-----------------+-----------------------------+----------------------------------------------------------------------------------------+
|   Parameter     |     Value                   |            Description                                                                 |
+=================+=============================+========================================================================================+
|                 | disabled                    | This value disables the serial port completely.                                        |
|                 +-----------------------------+----------------------------------------------------------------------------------------+
| ``uart=``       | bdf@<BDF value>             | This value sets the serial port PCI BDF (in HEX), for example, ``bdf@0xc1``.           |
|                 |                             |                                                                                        |
|                 |                             | BDF: Bus, Device, and Function (in HEX) of the serial PCI device. The BDF is packed    |
|                 |                             | into a 16-bit WORD with format (B:8, D:5, F:3). For example, PCI device ``0:18.1``     |
|                 |                             | becomes ``0xc1``.                                                                      |
|                 +-----------------------------+----------------------------------------------------------------------------------------+
|                 | port@<port address>         | This value sets the serial port PIO address, for example, ``uart=port@0x3F8``.         |
|                 +-----------------------------+----------------------------------------------------------------------------------------+
|                 | mmio@<MMIO address>         | This value sets the serial port MMIO address, for example, ``uart=mmio@0xfe040000``.   |
+-----------------+-----------------------------+----------------------------------------------------------------------------------------+

The Generic hypervisor parameters are specified in the GRUB multiboot/multiboot2 command.
For example:

.. code-block:: none
   :emphasize-lines: 5

   menuentry 'Boot ACRN hypervisor from multiboot' {
      insmod part_gpt
      insmod ext2
      echo 'Loading ACRN hypervisor ...'
      multiboot --quirk-modules-after-kernel /boot/acrn.32.out uart=bdf@0xc1
      module /boot/bzImage Linux_bzImage
      module /boot/bzImage2 Linux_bzImage2
   }

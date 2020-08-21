.. _hv-parameters:

ACRN Hypervisor Parameters
##########################

Generic hypervisor parameters
*****************************

The ACRN hypervisor supports the following parameter:

+-----------------+-----------------------------+----------------------------------------------------------------------------------------+
|   Parameter     |     Value                   |            Description                                                                 |
+=================+=============================+========================================================================================+
|                 | disabled                    | This disables the serial port completely.                                              |
|                 +-----------------------------+----------------------------------------------------------------------------------------+
|   uart=         | bdf@<BDF value>             | This sets the PCI serial port based on its BDF. e.g. bdf@0:18.1                        |
|                 +-----------------------------+----------------------------------------------------------------------------------------+
|                 | port@<port address>         | This sets the serial port address.                                                     |
+-----------------+-----------------------------+----------------------------------------------------------------------------------------+

The Generic hypervisor parameters are specified in the GRUB multiboot/multiboot2 command.
For example:

   .. code-block:: none
      :emphasize-lines: 5

      menuentry 'Boot ACRN hypervisor from multiboot' {
         insmod part_gpt
         insmod ext2
         echo 'Loading ACRN hypervisor ...'
         multiboot --quirk-modules-after-kernel /boot/acrn.32.out uart=bdf@0:18.1
         module /boot/bzImage Linux_bzImage
         module /boot/bzImage2 Linux_bzImage2
      }

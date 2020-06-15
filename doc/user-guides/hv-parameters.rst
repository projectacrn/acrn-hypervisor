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

For de-privilege mode, the parameters are specified in the ``efibootmgr -u`` command:

   .. code-block:: none
      :emphasize-lines: 2

      $ sudo efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L "ACRN NUC Hypervisor" \
            -u "uart=disabled"


De-privilege mode hypervisor parameters
***************************************

The de-privilege mode hypervisor parameters can only be specified in the efibootmgr command.
Currently we support the ``bootloader=`` parameter:

+-----------------+-------------------------------------------------+-------------------------------------------------------------------------+
|   Parameter     |     Value                                       |            Description                                                  |
+=================+=================================================+=========================================================================+
| bootloader=     | ``\EFI\org.clearlinux\bootloaderx64.efi``       | This sets the EFI executable to be loaded once the hypervisor is up     |
|                 |                                                 | and running. This is typically the bootloader of the Service OS.        |
|                 |                                                 | i.e. : ``\EFI\org.clearlinux\bootloaderx64.efi``                        |
+-----------------+-------------------------------------------------+-------------------------------------------------------------------------+

For example:

   .. code-block:: none
      :emphasize-lines: 2

      $ sudo efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L "ACRN NUC Hypervisor" \
            -u "bootloader=\EFI\boot\bootloaderx64.efi"

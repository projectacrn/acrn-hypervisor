.. _using_grub:

Getting Started Guide for Using GRUB
####################################
`GRUB <http://www.gnu.org/software/grub/>`_ is a multiboot boot loader. It supports boot ACRN
hypervisor on different working scenarios, it is also widely used for Linux distributions like
Ubuntu/Debian/Fedora/CentOS. Please check `<http://www.gnu.org/software/grub/grub-download.html>`_
to get the latest source code and `<https://www.gnu.org/software/grub/grub-documentation.html>`_
for the detailed documentations.

ACRN hypervisor could boot from `multiboot protocol <http://www.gnu.org/software/grub/manual/multiboot/multiboot.html>`_
or `multiboot2 protocol <http://www.gnu.org/software/grub/manual/multiboot2/multiboot.html>`_.
Comparing with multiboot protocol, the multiboot2 protocol adds UEFI support.

The multiboot protocol is supported in ACRN hypervisor by default, whereas multiboot2 is supported
when ``CONFIG_MULTIBOOT2`` is enabled in Kconfig. When hypervisor is loaded by GRUB ``multiboot``
command, the multiboot protocol is used, and guest kernel or ramdisk must be loaded with GRUB
``module`` command. When hypervisor is loaded by GRUB ``multiboot2`` command, the multiboot2 protocol
is used, and guest kernel or ramdisk must be loaded with GRUB ``module2`` command.

The ACRN hypervisor binary is built with two formats, ``acrn.32.out`` in ELF format and ``acrn.bin``
in RAW format. The GRUB ``multiboot`` command support ELF format only and does not support
binary relocation, whatever ``CONFIG_RELOC`` is set or not. The GRUB ``multiboot2`` command
support ELF format when ``CONFIG_RELOC`` is not set, or RAW format when ``CONFIG_RELOC`` is
set.

.. note::
   ``CONFIG_RELOC`` is set by default, so please use ``acrn.32.out`` in multiboot protocol
   or ``acrn.bin`` in multiboot2 protocol.

   To make sure platform get correct ACPI RSDP info, we recommand to use ``acrn.bin`` to load
   ACRN hypervisor on a UEFI platform.


Using pre-installed GRUB
************************

Most Linux distributions support GRUB version 2, we can re-use pre-installed GRUB to load ACRN hypervisor.
The prerequisites is GRUB version must be higher than 2.02.
Here we take Ubuntu for example to load ACRN on a scenario which has two pre-launched VMs (please
note SOS_VM is also a kind of pre-launched VM):

#. Copy ACRN hypervisor binary acrn.32.out(or acrn.bin) and pre-launched VM kernel images to ``/boot/``;

#. Modify the ``/etc/default/grub`` file as follows to make the GRUB menu visible when booting:

   .. code-block:: none

      # GRUB_HIDDEN_TIMEOUT=0
      GRUB_HIDDEN_TIMEOUT_QUIET=false

#. Append the following configuration in the ``/etc/grub.d/40_custom`` file (or ``/boot/grub/custom.cfg``
   file without following ``update-grub`` step):

   Configuration template for multiboot protocol:

   .. code-block:: none

      menuentry 'Boot ACRN hypervisor from multiboot' {
         insmod part_gpt
         insmod ext2
         echo 'Loading ACRN hypervisor ...'
         multiboot --quirk-modules-after-kernel /boot/acrn.32.out $(HV bootargs) $(SOS bootargs)
         module /boot/kernel4vm0 xxxxxx $(VM0 bootargs)
         module /boot/kernel4vm1 yyyyyy $(VM1 bootargs)
      }

   Configuration template for multiboot2 protocol:

   .. code-block:: none

      menuentry 'Boot ACRN hypervisor from multiboot2' {
         insmod part_gpt
         insmod ext2
         echo 'Loading ACRN hypervisor ...'
         multiboot2 /boot/acrn.bin $(HV bootargs) $(SOS bootargs)
         module2 /boot/kernel4vm0 xxxxxx $(VM0 bootargs)
         module2 /boot/kernel4vm1 yyyyyy $(VM1 bootargs)
      }


   .. note::
      The module ``/boot/kernel4vm0`` is the VM0 kernel file. The param ``xxxxxx`` is
      VM0's kernel file tag and must exactly match the ``kernel_mod_tag`` of VM0 which
      is configured in the ``hypervisor/scenarios/$(SCENARIO)/vm_configurations.c``
      file. The multiboot module ``/boot/kernel4vm1`` is the VM1 kernel file and the param
      ``yyyyyy`` is its tag and must exactly match the ``kernel_mod_tag`` of VM1 in the
      ``hypervisor/scenarios/$(SCENARIO)/vm_configurations.c`` file.

      The guest kernel command line arguments is configured in hypervisor source code by default
      if no ``$(VMx bootargs)`` presents, but if ``$(VMx bootargs)`` presents, the default
      command line arguments would be overridden by the ``$(VMx bootargs)`` parameters.

      The ``$(SOS bootargs)`` parameter in multiboot command would be appended to the end of Service
      VM kernel command line. This is useful to override some SOS kernel cmdline parameters because
      the later one would win if same parameters configured in Linux kernel cmdline. For example,
      add ``root=/dev/sda3`` will override the original root device to ``/dev/sda3`` for SOS kernel.

      All parameters after ``#`` character would be ignored since GRUB treat them as comments.

      ``\``, ``$``, ``#`` are special characters in GRUB, a escape character ``\`` need to be added
      before these special charactors if they are included in ``$(HV bootargs)`` or ``$(VM bootargs)``.
      e.g. ``memmap=0x200000$0xE00000`` for guest kernel cmdline need to be changed to
      ``memmap=0x200000\$0xE00000``


#. Update GRUB::

   $ sudo update-grub

#. Reboot the platform. Select the **Boot ACRN hypervisor xxx** entry to boot the ACRN hypervisor on the
   platform's display. The GRUB loader will boot the hypervisor, and the hypervisor will start the VMs automatically.

   
Installing self-built GRUB
**************************

If the GRUB version on your platform is outdated or has issue to boot ACRN hypervisor, you can have
a try with self-built GRUB binary. Please get the latest GRUB code and follow `GRUB Manual
<https://www.gnu.org/software/grub/manual/grub/grub.html#Installing-GRUB-using-grub_002dinstall>`_
to install your own GRUB, and then follow steps in **Using pre-installed GRUB**.

Here we provide another simple method to build GRUB in efi application format:

#. make GRUB efi application:

   .. code-block:: none

      $ git clone https://git.savannah.gnu.org/git/grub.git
      $ cd grub
      $ ./bootstrap
      $ ./configure --with-platform=efi --target=x86_64
      $ make
      $ ./grub-mkimage -p /EFI/BOOT -d ./grub-core/ -O x86_64-efi -o grub_x86_64.efi \
            boot efifwsetup efi_gop efinet efi_uga lsefimmap lsefi lsefisystab \
            exfat fat multiboot2 multiboot terminal part_msdos part_gpt normal \
            all_video aout configfile echo file fixvideo fshelp gfxterm gfxmenu \
            gfxterm_background gfxterm_menu legacycfg video_bochs video_cirrus \
            video_colors video_fb videoinfo video net tftp

   This will build a ``grub_x86_64.efi`` binary in the current directory, copy it to ``/EFI/boot/`` folder
   on EFI partition.

#. create ``/EFI/boot/grub.cfg`` file containing the following:

   .. code-block:: none

      set default=0
      set timeout=5
      # set correct root device which stores acrn binary and kernel images
      set root='hd0,gpt3'

      menuentry 'Boot ACRN hypervisor from multiboot' {
         insmod part_gpt
         insmod ext2
         echo 'Loading ACRN hypervisor ...'
         multiboot --quirk-modules-after-kernel /boot/acrn.32.out $(HV bootargs) $(SOS bootargs)
         module /boot/kernel4vm0 xxxxxx $(VM0 bootargs)
         module /boot/kernel4vm1 yyyyyy $(VM1 bootargs)
      }

      menuentry 'Boot ACRN hypervisor from multiboot2' {
         insmod part_gpt
         insmod ext2
         echo 'Loading ACRN hypervisor ...'
         multiboot2 /boot/acrn.bin $(HV bootargs) $(SOS bootargs)
         module2 /boot/kernel4vm0 xxxxxx $(VM0 bootargs)
         module2 /boot/kernel4vm1 yyyyyy $(VM1 bootargs)
      }

#. copy ACRN binary and guest kernel images to the folder which GRUB configures, e.g. ``/boot/`` folder on ``/dev/sda3/``;

#. run ``/EFI/boot/grub_x86_64.efi`` in EFI shell.

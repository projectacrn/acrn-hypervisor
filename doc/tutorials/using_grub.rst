.. _using_grub:

Using GRUB to boot ACRN
#######################
`GRUB <http://www.gnu.org/software/grub/>`_ is a multiboot boot loader
used by many popular Linux distributions. It also supports booting the
ACRN hypervisor.
See `<http://www.gnu.org/software/grub/grub-download.html>`_
to get the latest GRUB source code and `<https://www.gnu.org/software/grub/grub-documentation.html>`_
for detailed documentation.

The ACRN hypervisor can boot from `multiboot protocol <http://www.gnu.org/software/grub/manual/multiboot/multiboot.html>`_
or `multiboot2 protocol <http://www.gnu.org/software/grub/manual/multiboot2/multiboot.html>`_.
Comparing with multiboot protocol, the multiboot2 protocol adds UEFI support.

The multiboot protocol is supported by the ACRN hypervisor natively.
The multiboot2 protocol is supported when ``CONFIG_MULTIBOOT2`` is
enabled in Kconfig. The ``CONFIG_MULTIBOOT2`` is enabled by default.
Which boot protocol is used depends on the hypervisor is loaded by
GRUB's ``multiboot`` command or ``multiboot2`` command. The guest kernel
or ramdisk must be loaded by the GRUB ``module`` command or ``module2``
command accordingly when different boot protocol is used.

The ACRN hypervisor binary is built with two formats: ``acrn.32.out`` in
ELF format and ``acrn.bin`` in RAW format. The GRUB ``multiboot``
command support ELF format only and does not support binary relocation,
even if ``CONFIG_RELOC`` is set. The GRUB ``multiboot2`` command support
ELF format when ``CONFIG_RELOC`` is not set, or RAW format when
``CONFIG_RELOC`` is set.

.. note::
   * ``CONFIG_RELOC`` is set by default, so use ``acrn.32.out`` in multiboot
     protocol and ``acrn.bin`` in multiboot2 protocol.

   * Per ACPI specification, the RSDP pointer is described in the EFI System
     Table instead of legacy ACPI RSDP area on a UEFI enabled platform. To make
     sure ACRN hypervisor gets the correct ACPI RSDP info, we recommend using
     ``acrn.bin`` with multiboot2 protocol to load hypervisor on a UEFI platform.

.. _pre-installed-grub:

Using pre-installed GRUB
************************

Most Linux distributions use GRUB version 2 by default. We can re-use
pre-installed GRUB to load ACRN hypervisor if its version 2.02 or
higher.

Here's an example using Ubuntu to load ACRN on a scenario with two
pre-launched VMs (the SOS_VM is also a kind of pre-launched VM):

#. Copy ACRN hypervisor binary ``acrn.32.out`` (or ``acrn.bin``) and the pre-launched VM kernel images to ``/boot/``;

#. Modify the ``/etc/default/grub`` file as follows to make the GRUB menu visible when booting:

   .. code-block:: none

      # GRUB_HIDDEN_TIMEOUT=0
      GRUB_HIDDEN_TIMEOUT_QUIET=false

#. Append the following configuration in the ``/etc/grub.d/40_custom`` file:

   Configuration template for multiboot protocol:

   .. code-block:: none

      menuentry 'Boot ACRN hypervisor from multiboot' {
         insmod part_gpt
         insmod ext2
         echo 'Loading ACRN hypervisor ...'
         multiboot --quirk-modules-after-kernel /boot/acrn.32.out $(HV bootargs) $(Service VM bootargs)
         module /boot/kernel4vm0 xxxxxx $(VM0 bootargs)
         module /boot/kernel4vm1 yyyyyy $(VM1 bootargs)
      }

   Configuration template for multiboot2 protocol:

   .. code-block:: none

      menuentry 'Boot ACRN hypervisor from multiboot2' {
         insmod part_gpt
         insmod ext2
         echo 'Loading ACRN hypervisor ...'
         multiboot2 /boot/acrn.bin $(HV bootargs) $(Service VM bootargs)
         module2 /boot/kernel4vm0 xxxxxx $(VM0 bootargs)
         module2 /boot/kernel4vm1 yyyyyy $(VM1 bootargs)
      }


   .. note::
      The module ``/boot/kernel4vm0`` is the VM0 kernel file. The param
      ``xxxxxx`` is VM0's kernel file tag and must exactly match the
      ``kernel_mod_tag`` of VM0 configured in the
      ``misc/vm_configs/scenarios/$(SCENARIO)/vm_configurations.c`` file. The
      multiboot module ``/boot/kernel4vm1`` is the VM1 kernel file and the
      param ``yyyyyy`` is its tag and must exactly match the
      ``kernel_mod_tag`` of VM1 in the
      ``misc/vm_configs/scenarios/$(SCENARIO)/vm_configurations.c`` file.

      The guest kernel command line arguments is configured in the
      hypervisor source code by default if no ``$(VMx bootargs)`` is present.
      If ``$(VMx bootargs)`` is present, the default command line arguments
      are overridden by the ``$(VMx bootargs)`` parameters.

      The ``$(Service VM bootargs)`` parameter in the multiboot command
      is appended to the end of the Service VM kernel command line. This is
      useful to override some Service VM kernel cmdline parameters because the
      later one would win if the same parameters were configured in the Linux
      kernel cmdline. For example, adding ``root=/dev/sda3`` will override the
      original root device to ``/dev/sda3`` for the Service VM kernel.

      All parameters after a ``#`` character are ignored since GRUB
      treat them as comments.

      ``\``, ``$``, ``#`` are special characters in GRUB. An escape
      character ``\`` must be added before these special characters if they
      are included in ``$(HV bootargs)`` or ``$(VM bootargs)``.  For example,
      ``memmap=0x200000$0xE00000`` for guest kernel cmdline must be written as
      ``memmap=0x200000\$0xE00000``


#. Update GRUB::

   $ sudo update-grub

#. Reboot the platform. On the platform's console, Select the
   **Boot ACRN hypervisor xxx** entry to boot the ACRN hypervisor.
   The GRUB loader will boot the hypervisor, and the hypervisor will
   start the VMs automatically.


Installing self-built GRUB
**************************

If the GRUB version on your platform is outdated or has issues booting
the ACRN hypervisor, you can have a try with self-built GRUB binary. Get
the latest GRUB code and follow the `GRUB Manual
<https://www.gnu.org/software/grub/manual/grub/grub.html#Installing-GRUB-using-grub_002dinstall>`_
to build and install your own GRUB, and then follow the steps described
earlier in `pre-installed-grub`_.


Here we provide another simple method to build GRUB in efi application format:

#. Make GRUB efi application:

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
   on the EFI partition (it is typically mounted under ``/boot/efi/`` folder on rootfs).

#. Create ``/EFI/boot/grub.cfg`` file containing the following:

   .. code-block:: none

      set default=0
      set timeout=5
      # set correct root device which stores acrn binary and kernel images
      set root='hd0,gpt3'

      menuentry 'Boot ACRN hypervisor from multiboot' {
         insmod part_gpt
         insmod ext2
         echo 'Loading ACRN hypervisor ...'
         multiboot --quirk-modules-after-kernel /boot/acrn.32.out $(HV bootargs) $(Service VM bootargs)
         module /boot/kernel4vm0 xxxxxx $(VM0 bootargs)
         module /boot/kernel4vm1 yyyyyy $(VM1 bootargs)
      }

      menuentry 'Boot ACRN hypervisor from multiboot2' {
         insmod part_gpt
         insmod ext2
         echo 'Loading ACRN hypervisor ...'
         multiboot2 /boot/acrn.bin $(HV bootargs) $(Service VM bootargs)
         module2 /boot/kernel4vm0 xxxxxx $(VM0 bootargs)
         module2 /boot/kernel4vm1 yyyyyy $(VM1 bootargs)
      }

#. Copy ACRN binary and guest kernel images to the GRUB-configured folder, e.g. ``/boot/`` folder on ``/dev/sda3/``;

#. Run ``/EFI/boot/grub_x86_64.efi`` in the EFI shell.
